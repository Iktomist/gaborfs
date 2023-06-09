/*
    segment.c -- filesystem relocation functions; segment functions.
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <reiserfs/reiserfs.h>
#include <reiserfs/debug.h>

#include <dal/dal.h>

#define N_(String) (String)
#if ENABLE_NLS
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif

struct reiserfs_reloc_desc {
    int smart;
    reiserfs_segment_t *dst_segment;
    reiserfs_segment_t *src_segment;
    reiserfs_fs_t *dst_fs;
    reiserfs_fs_t *src_fs;
	
    reiserfs_gauge_t *gauge;
    blk_t counter;
};

static blk_t generic_node_write(struct reiserfs_reloc_desc *reloc, reiserfs_block_t *node) {
    blk_t dst_blk, offset;
    reiserfs_fs_t *src_fs, *dst_fs;
	
    src_fs = reloc->src_fs;
    dst_fs = reloc->dst_fs;

    if (!reloc->smart && dal_equals(dst_fs->dal, src_fs->dal)) {
	if (reiserfs_segment_test_inside(reloc->dst_segment, reiserfs_block_get_nr(node)))
	    return reiserfs_block_get_nr(node);
    }
	
    if (reloc->gauge) {
	libreiserfs_gauge_set_value(reloc->gauge, (reloc->counter++ * 100) / 
	    reiserfs_segment_len(reloc->src_segment));
    }
    
    if (dal_equals(dst_fs->dal, src_fs->dal))
	reiserfs_fs_bitmap_unuse_block(src_fs, reiserfs_block_get_nr(node) - 
	labs(src_fs->tree->offset));
	
    offset = (reloc->smart ? (reloc->dst_segment->start - reloc->src_segment->start) : 0);
	
    /* Finding free block */
    if (!(dst_blk = reiserfs_fs_bitmap_find_free_block(dst_fs, reloc->dst_segment->start - 
	(reloc->src_segment->start < reloc->dst_segment->start ? offset : 0)))) 
    {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't find free block inside allowed area (%lu - %lu)."), 
	    reloc->dst_segment->start, reloc->dst_segment->end);
	return 0;
    }

    reiserfs_block_set_nr(node, dst_blk + (reloc->src_segment->start < 
	    reloc->dst_segment->start ? offset : 0));
	
    reiserfs_fs_bitmap_use_block(dst_fs, dst_blk);

    if (!reiserfs_block_write(dst_fs->dal, node)) {
	reiserfs_block_writing_failed(reiserfs_block_get_nr(node),
	    dal_error(dst_fs->dal), return 0);
    }
	
    return dst_blk;
}

/* Calback functions */
static long callback_node_check(reiserfs_block_t *node, struct reiserfs_reloc_desc *reloc) {
	(void)node;
	(void)reloc;

    /* 
	Some node checks must be here. This is because traverse function 
	performs only basic checks like tree node level check.
    */
    return 1;
}    

static long callback_node_setup(reiserfs_block_t *node, struct reiserfs_reloc_desc *reloc) {
    reiserfs_item_head_t *item;
    reiserfs_fs_t *dst_fs, *src_fs;
	
    src_fs = reloc->src_fs;
    dst_fs = reloc->dst_fs;
	
    if (reloc->gauge) {
	libreiserfs_gauge_set_value(reloc->gauge, (reloc->counter++ * 100) / 
	    reiserfs_segment_len(reloc->src_segment));
    }
	
    if (is_leaf_node(node)) {
	uint32_t i;

	for (i = 0; i < get_node_nritems(get_node_head(node)); i++) {
	    item = get_ih_item_head(node, i);

	    if (!dal_equals(src_fs->dal, dst_fs->dal)) {
		if (is_stat_data_ih(item))
		    reiserfs_object_use(dst_fs, get_key_objid(&item->ih_key));
	    }
	    
	    /* Moving the all pieces of a big file */
	    if (is_indirect_ih(item)) {
		uint32_t unfm, *blocks = (uint32_t *)get_ih_item_body(node, item);
	    
		for (unfm = 0; unfm < get_ih_unfm_nr(item); unfm++) {
		    if (blocks[unfm] != 0) {
			reiserfs_block_t *node;
			blk_t blk = LE32_TO_CPU(blocks[unfm]) + src_fs->tree->offset;
								
			if (!(node = reiserfs_block_read(src_fs->dal, blk)))
			    reiserfs_block_reading_failed(blk, dal_error(src_fs->dal), return 0);
							
			if (!(blk = generic_node_write(reloc, node))) {
			    reiserfs_block_free(node);
			    return 0;
			}
			blocks[unfm] = CPU_TO_LE32(blk);
			reiserfs_block_free(node);
		    }
		}
	    }
	}
	reiserfs_block_mark_dirty(node);
    }
    return reiserfs_block_get_nr(node);
}

static long callback_chld_setup(reiserfs_block_t *node, uint32_t chld, long chld_blk, 
    struct reiserfs_reloc_desc *reloc)
{
	(void)reloc;

    set_dc_child_blocknr(get_node_disk_child(node, chld), (blk_t)chld_blk);
    reiserfs_block_mark_dirty(node);
    return 1;
}

static long callback_node_write(reiserfs_block_t *node, struct reiserfs_reloc_desc *reloc) {
    reiserfs_fs_t *src_fs, *dst_fs;
	
    src_fs = reloc->src_fs;
    dst_fs = reloc->dst_fs;
	
    if (!reloc->smart && dal_equals(dst_fs->dal, src_fs->dal)) {
	blk_t node_loc = reiserfs_block_get_nr(node);
	if (reiserfs_segment_test_inside(reloc->dst_segment, node_loc) &&
		!reiserfs_block_dirty(node))
	    return node_loc;
    }

    /* Moving given node to found free location */
    return generic_node_write(reloc, node);
}

/* Geometry functions */
reiserfs_segment_t *reiserfs_segment_create(dal_t *dal, blk_t start, blk_t end) {
    reiserfs_segment_t *segment;
	
    if (!(segment = libreiserfs_calloc(sizeof(*segment), 0)))
	return NULL;

    if (!reiserfs_segment_init(segment, dal, start, end))
	goto error_free_segment;
	
    return segment;
	
error_free_segment:
    libreiserfs_free(segment);
error:
    return NULL;
}
	
int reiserfs_segment_init(reiserfs_segment_t *segment, dal_t *dal, blk_t start, blk_t end) {
    ASSERT(dal != NULL, return 0);
    ASSERT(segment != NULL, return 0);
	
    segment->dal = dal;
    segment->start = start;
    segment->end = end;

    return 1;
}
	
void reiserfs_segment_free(reiserfs_segment_t *segment) {
    ASSERT(segment != NULL, return);
    libreiserfs_free(segment);
}

blk_t reiserfs_segment_relocate(reiserfs_fs_t *dst_fs, reiserfs_segment_t *dst_segment, 
    reiserfs_fs_t *src_fs, reiserfs_segment_t *src_segment, int smart) 
{
    struct reiserfs_reloc_desc reloc;
	
    ASSERT(dst_segment != NULL, return 0);
    ASSERT(src_segment != NULL, return 0);
    ASSERT(dst_fs != NULL, return 0);
    ASSERT(src_fs != NULL, return 0);
	
    reloc.dst_segment = dst_segment;
    reloc.src_segment = src_segment;
    reloc.gauge = libreiserfs_get_gauge();
    reloc.dst_fs = dst_fs;
    reloc.src_fs = src_fs;
    reloc.smart = smart;
    reloc.counter = 0;

    return reiserfs_tree_traverse(reiserfs_fs_tree(src_fs), &reloc, 
	(reiserfs_edge_traverse_func_t)callback_node_check, 
	(reiserfs_node_func_t)callback_node_setup, 
	(reiserfs_chld_func_t)callback_chld_setup, 
	(reiserfs_edge_traverse_func_t)callback_node_write);
}

int reiserfs_segment_test_inside(reiserfs_segment_t *segment, blk_t blk) {
    ASSERT(segment != NULL, return 0);
    return (blk >= segment->start && blk < segment->end);
}

int reiserfs_segment_test_overlap(reiserfs_segment_t *segment1, reiserfs_segment_t *segment2) {
    ASSERT(segment1 != NULL, return 0);
    ASSERT(segment2 != NULL, return 0);
   
    if (dal_equals(segment1->dal, segment2->dal)) {
	if (segment1->start < segment2->start)
	    return segment1->end > segment2->start;
	else
	    return segment2->end > segment1->start;
    }
	
    return 0;
}

#include <stdio.h>

int reiserfs_segment_move(reiserfs_segment_t *dst_segment, reiserfs_segment_t *src_segment, 
    reiserfs_segment_func_t segment_func, void *data) 
{
    blk_t i;
    reiserfs_block_t *block;
	
    ASSERT(src_segment != NULL, return 0);
    ASSERT(dst_segment != NULL, return 0);
	
    i = (src_segment->start < dst_segment->start ? 
		    reiserfs_segment_len(src_segment) - 1 : 0);
    
    while (1) {
	if (!(block = reiserfs_block_read(src_segment->dal, 
					src_segment->start + i))) 
	{
	    reiserfs_block_reading_failed(src_segment->start + i, 
		dal_error(src_segment->dal), return 0);
	}
		
	printf("Moving block %u to %u\n", src_segment->start + i,
			dst_segment->start + i);

	reiserfs_block_set_nr(block, dst_segment->start + i);

	if (!reiserfs_block_write(dst_segment->dal, block)) {
	    reiserfs_block_writing_failed(dst_segment->start + i, 
		dal_error(dst_segment->dal), goto error_free_block);
	}
	
	if (segment_func) {
	    if (!segment_func(src_segment, block, 
				    (src_segment->start < dst_segment->start ? 
		reiserfs_segment_len(src_segment) - i : i), data))
	    goto error_free_block;
	}
	reiserfs_block_free(block);
	
	if ((src_segment->start < dst_segment->start && i-- == 0) || 
		(src_segment->start >= dst_segment->start && i++ == 
		reiserfs_segment_len(src_segment) - 1))
	    break;
    }
	
    return 1;
	
error_free_block:
    reiserfs_block_free(block);
error:
    return 0;
}

int reiserfs_segment_fill(reiserfs_segment_t *segment, char c, 
    reiserfs_segment_func_t segment_func, void *data)
{
    blk_t i;
    reiserfs_block_t *block;
	
    ASSERT(segment != NULL, return 0);

    for (i = 0; i < reiserfs_segment_len(segment); i++) {

    	if (!(block = reiserfs_block_alloc(segment->dal, segment->start + i, c)))
	    return 0;
		
	if (!reiserfs_block_write(segment->dal, block)) {
	    reiserfs_block_writing_failed(segment->start + i, 
		dal_error(segment->dal), goto error_free_block);
	}

	if (segment_func) {
	    if (!segment_func(segment, block, i, data))
		goto error_free_block;
	}	
	reiserfs_block_free(block);
    }

    return 1;
error_free_block:
    reiserfs_block_free(block);
error:
    return 0;
}

blk_t reiserfs_segment_len(reiserfs_segment_t *segment) {
    ASSERT(segment != NULL, return 0);
    return segment->end - segment->start;
}

