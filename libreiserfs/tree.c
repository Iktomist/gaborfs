/*
    tree.c -- reiserfs balanced tree implementation
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>

#include <reiserfs/reiserfs.h>
#include <reiserfs/debug.h>

#define N_(String) (String)
#if ENABLE_NLS
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif

#define get_st_blocks(size) ((size + 511) / 512)

static int reiserfs_tree_internal_insert(reiserfs_block_t *node, 
    uint32_t pos, struct key *key, reiserfs_disk_child_t *child);

static reiserfs_block_t *reiserfs_tree_node_alloc(reiserfs_tree_t *tree, 
    uint32_t level) 
{
    blk_t blk;
    reiserfs_block_t *node;

    if (!(blk = reiserfs_fs_bitmap_find_free_block(tree->fs, 1))) {
        libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't find free block."));
	return 0;
    }
	
    if (!(node = reiserfs_block_alloc(reiserfs_tree_dal(tree), blk, 0)))
        return 0;
	    
    set_node_level(get_node_head(node), level);
    set_node_nritems(get_node_head(node), 0);
    set_node_free_space(get_node_head(node), reiserfs_fs_block_size(tree->fs) - 
    	NDHD_SIZE);
	
    return node;
}

dal_t *reiserfs_tree_dal(reiserfs_tree_t *tree) {
    ASSERT(tree != NULL, return NULL);
    return tree->fs->dal;
}

blk_t reiserfs_tree_get_root(reiserfs_tree_t *tree) {
    ASSERT(tree != NULL, return 0);
    return get_sb_root_block(tree->fs->super);
}

void reiserfs_tree_set_root(reiserfs_tree_t *tree, blk_t root) {
    ASSERT(tree != NULL, return);
	
    set_sb_root_block(tree->fs->super, root);
    reiserfs_fs_mark_super_dirty(tree->fs);
}

uint32_t reiserfs_tree_get_height(reiserfs_tree_t *tree) {
    ASSERT(tree != NULL, return 0);
    return get_sb_tree_height(tree->fs->super);
}

void reiserfs_tree_set_height(reiserfs_tree_t *tree, uint32_t height) {
    ASSERT(tree != NULL, return);
    ASSERT(height < MAX_HEIGHT, return);
	
    set_sb_tree_height(tree->fs->super, height);
    reiserfs_fs_mark_super_dirty(tree->fs);
}

reiserfs_tree_t *reiserfs_tree_open(reiserfs_fs_t *fs) {
    reiserfs_tree_t *tree;
	
    ASSERT(fs != NULL, return NULL);
    
    if (!(tree = (reiserfs_tree_t *)libreiserfs_calloc(sizeof(*tree), 0)))
	return NULL;
	
    tree->fs = fs;
    return tree;
}

static inline void make_empty_direntry(reiserfs_de_head_t *deh, int format, 
	uint32_t dirid, uint32_t objid, uint32_t par_dirid, uint32_t par_objid)
{
    size_t dir_size;

    dir_size = (size_t)(format == FS_FORMAT_3_6 ? EMPTY_DIR_V2_SIZE : EMPTY_DIR_V1_SIZE);
	
    memset(deh, 0, dir_size);
	
    /* Direntry header of "." */
    set_de_offset(&deh[0], DOT_OFFSET);
    set_de_dirid(&deh[0], dirid);
    set_de_objid(&deh[0], objid);

    if (format == FS_FORMAT_3_6)
	set_de_location(&deh[0], (EMPTY_DIR_V2_SIZE - ROUND_UP(strlen("."))));
    else	
	set_de_location(&deh[0], (EMPTY_DIR_V1_SIZE - strlen(".")));
	
    set_de_state(&deh[0], 0);
    mark_de_visible(&deh[0]);
  
    /* Direntry header of ".." */
    set_de_offset(&deh[1], DOT_DOT_OFFSET);
	
    /* Key of ".." for the directory */
    set_de_dirid(&deh[1], par_dirid);
    set_de_objid(&deh[1], par_objid);
	
    if (format == FS_FORMAT_3_6)
    	set_de_location(&deh[1], (get_de_location(&deh[0]) - ROUND_UP(strlen(".."))));
    else	
    	set_de_location(&deh[1], (get_de_location(&deh[0]) - strlen("..")));
	
    set_de_state(&deh[1], 0);
    mark_de_visible(&deh[1]);
    
    memcpy((void *)deh + get_de_location(&deh[0]), ".", 1);
    memcpy((void *)deh + get_de_location(&deh[1]), "..", 2);
}

static void make_empty_dir(void *body, int format, size_t blocksize, 
	uint32_t dirid, uint32_t objid, uint32_t par_dirid, uint32_t par_objid) 
{
    reiserfs_item_head_t *ih;
    reiserfs_sd_v1_t *sd_v1;
    reiserfs_sd_v2_t *sd_v2;

    ASSERT(body != NULL, return);
	
    /* First item is stat data item of the directory */
    ih = (reiserfs_item_head_t *)body;
    set_key_dirid(&ih->ih_key, dirid);
    set_key_objid(&ih->ih_key, objid);

    if (format == FS_FORMAT_3_6) {
    	set_ih_item_format(ih, ITEM_FORMAT_2);
    	set_key_v2_offset(&ih->ih_key, SD_OFFSET);
    	set_key_v2_type(&ih->ih_key, KEY_TYPE_SD);
    } else {
    	set_ih_item_format(ih, ITEM_FORMAT_1);
    	set_key_v1_offset(&ih->ih_key, SD_OFFSET);
    	set_key_v1_type(&ih->ih_key, KEY_UNIQ_SD);
    }	
    set_ih_item_len(ih, (format == FS_FORMAT_3_6 ?  SD_V2_SIZE : SD_V1_SIZE));
    set_ih_item_location(ih, blocksize - (format == FS_FORMAT_3_6 ? SD_V2_SIZE : 
	SD_V1_SIZE));

    set_ih_free_space(ih, MAX_US_INT);

    /* Fill new stat data */
    if (format == FS_FORMAT_3_6) {
	sd_v2 = (reiserfs_sd_v2_t *)(body + get_ih_item_location(ih) - NDHD_SIZE);
		
	set_sd_v2_mode(sd_v2, S_IFDIR + 0755);
	set_sd_v2_nlink(sd_v2, 3);
	set_sd_v2_uid(sd_v2, getuid());
	set_sd_v2_gid(sd_v2, getgid());
		
	set_sd_v2_size(sd_v2, EMPTY_DIR_V2_SIZE);
		
	set_sd_v2_atime(sd_v2, time(NULL));
	set_sd_v2_ctime(sd_v2, time(NULL));
	set_sd_v2_mtime(sd_v2, time(NULL));
		
	set_sd_v2_blocks(sd_v2, get_st_blocks(EMPTY_DIR_V2_SIZE));
	set_sd_v2_rdev(sd_v2, 0);
    } else {
	sd_v1 = (reiserfs_sd_v1_t *)(body + get_ih_item_location(ih) - NDHD_SIZE);
		
	set_sd_v1_mode(sd_v1, S_IFDIR + 0755);
	set_sd_v1_nlink(sd_v1, 3);
	set_sd_v1_uid(sd_v1, getuid());
	set_sd_v1_gid(sd_v1, getgid());
		
	set_sd_v1_size(sd_v1, EMPTY_DIR_V1_SIZE);
		
	set_sd_v1_atime(sd_v1, time(NULL));
	set_sd_v1_ctime(sd_v1, time(NULL));
	set_sd_v1_mtime(sd_v1, time(NULL));
		
	set_sd_v1_blocks(sd_v1, get_st_blocks(EMPTY_DIR_V1_SIZE));
    }

    /* Second item is directory item, containing "." and ".." */
    ih++;
    set_key_dirid(&ih->ih_key, dirid);
    set_key_objid(&ih->ih_key, objid);
	
    if (format == FS_FORMAT_3_6) {
    	set_ih_item_format(ih, ITEM_FORMAT_2);
    	set_key_v2_offset(&ih->ih_key, DOT_OFFSET);
    	set_key_v2_type(&ih->ih_key, KEY_TYPE_DR);
    } else {
    	set_ih_item_format(ih, ITEM_FORMAT_1);
    	set_key_v1_offset(&ih->ih_key, DOT_OFFSET);
	set_key_v1_type(&ih->ih_key, KEY_UNIQ_DR);
    }	
	
    set_ih_item_len(ih, 
	(format == FS_FORMAT_3_6 ? EMPTY_DIR_V2_SIZE : EMPTY_DIR_V1_SIZE));
    set_ih_item_location(ih, get_ih_item_location(ih - 1) - get_ih_item_len(ih));
    set_ih_entry_count(ih, 2);

    /* Compose item */
    make_empty_direntry((reiserfs_de_head_t *)(body + get_ih_item_location(ih) - 
	NDHD_SIZE), format, dirid, objid, 0, dirid);
}

reiserfs_tree_t *reiserfs_tree_create(reiserfs_fs_t *fs) {
    int format;
    blk_t root_blk;
    size_t blocksize;
    reiserfs_tree_t *tree;
    reiserfs_block_t *root;
    reiserfs_node_head_t *node;
	
    ASSERT(fs != NULL, return NULL);
	
    if (!(tree = (reiserfs_tree_t *)libreiserfs_calloc(sizeof(*tree), 0)))
	return NULL;
    tree->fs = fs;
	
    if (!(root = reiserfs_tree_node_alloc(tree, 2)))
	goto error_free_tree;
	
    blocksize = get_sb_block_size(fs->super);
    format = get_sb_format(fs->super);
	
    /* Block head */
    node = (reiserfs_node_head_t *)root->data;
    set_node_level(node, LEAF_LEVEL);
    set_node_nritems(node, 2);
	    
    set_node_free_space(node, blocksize - NDHD_SIZE - 2 * IH_SIZE - 
	(format == FS_FORMAT_3_6 ? SD_V2_SIZE : SD_V1_SIZE) - 
	(format == FS_FORMAT_3_6 ? EMPTY_DIR_V2_SIZE : EMPTY_DIR_V1_SIZE));

    make_empty_dir(root->data + NDHD_SIZE, format, 
	blocksize, ROOT_DIR_ID, ROOT_OBJ_ID, 0, ROOT_DIR_ID);

    if (!reiserfs_block_write(reiserfs_tree_dal(tree), root)) {
	reiserfs_block_writing_failed(root, 
	    dal_error(reiserfs_tree_dal(tree)), goto error_free_root);
    }
    
    root_blk = reiserfs_block_get_nr(root);
    reiserfs_fs_bitmap_use_block(tree->fs, root_blk);
	
    reiserfs_object_use(fs, ROOT_DIR_ID);
    reiserfs_object_use(fs, ROOT_OBJ_ID);
	
    reiserfs_tree_set_height(tree, 2);
    reiserfs_tree_set_root(tree, root_blk);
	
    reiserfs_block_free(root);
    return tree;

error_free_root:
    reiserfs_block_free(root);    
error_free_tree:
    libreiserfs_free(tree);    
error:
    return NULL;    
}

static int reiserfs_tree_node_lookup(reiserfs_tree_t *tree, blk_t blk, 
    reiserfs_comp_func_t comp_func, struct key *key, int for_leaf, 
    reiserfs_path_t *path) 
{
    reiserfs_block_t *node;
    uint32_t level, found = 0, pos = 0;
	
    ASSERT(tree != NULL, return 0);
    ASSERT(key != NULL, return 0);
	
    if (!comp_func) return 0;
	
    if (path)
	reiserfs_path_clear(path);
	
    while (1) {
	if (!(node = reiserfs_block_read(tree->fs->dal, blk)))
	    reiserfs_block_reading_failed(blk, dal_error(tree->fs->dal), return 0);
		
	if ((level = get_node_level((reiserfs_node_head_t *)node->data)) > 
	    (uint32_t)reiserfs_tree_get_height(tree) - 1)
	{
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		_("Invalid node level. Found %d, expected less than %d."), 
		level, reiserfs_tree_get_height(tree));
	    return 0;
	}

	if (!for_leaf && is_leaf_node(node))
	    return 0;
			
	found = reiserfs_tools_fast_search(key, get_ih_item_head(node, 0), 
	    get_node_nritems(get_node_head(node)), (is_leaf_node(node) ? 
	    IH_SIZE : FULL_KEY_SIZE), comp_func, &pos);
		
	if (path) {
	    if (!reiserfs_path_inc(path, 
		    reiserfs_path_node_create(reiserfs_path_last(path), node, 
		    (found && is_internal_node(node) ? pos + 1 : pos))))
		return 0;
	}
		
	if (is_leaf_node(node))
	    return found;
			
	if (level == 2 && !for_leaf)
	    return 1;
			
	if (found) pos++;
		
	blk = get_dc_child_blocknr(get_node_disk_child(node, pos)) + tree->offset;
    }
	
    return 0;
}

reiserfs_path_node_t *reiserfs_tree_lookup_internal(reiserfs_tree_t *tree, blk_t from,
	reiserfs_comp_func_t comp_func, struct key *key, reiserfs_path_t *path) 
{
    if (tree && reiserfs_tree_get_height(tree) < 2)
	return NULL;

    return (reiserfs_tree_node_lookup(tree, from, comp_func, key, 0, path) ? 
	reiserfs_path_last(path) : NULL);
}

reiserfs_path_node_t *reiserfs_tree_lookup_leaf(reiserfs_tree_t *tree, blk_t from,
    reiserfs_comp_func_t comp_func, struct key *key, reiserfs_path_t *path) 
{
    if (tree && reiserfs_tree_get_height(tree) < 2)
	return NULL;
	
    return (reiserfs_tree_node_lookup(tree, from, comp_func, key, 1, path) ? 
	reiserfs_path_last(path) : NULL);
}

static long reiserfs_tree_node_traverse(reiserfs_tree_t *tree, blk_t blk, void *data,
    reiserfs_edge_traverse_func_t before_node_func, reiserfs_node_func_t node_func, 
    reiserfs_chld_func_t chld_func, reiserfs_edge_traverse_func_t after_node_func)
{
    uint32_t block_level;
    long call_result = 0;
    reiserfs_block_t *node;
	
    if (!node_func) return 0;

    if (!(node = reiserfs_block_read(tree->fs->dal, blk)))
	reiserfs_block_reading_failed(blk, dal_error(tree->fs->dal), goto error);
	
    if (!is_leaf_node(node) && !is_internal_node(node)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Invalid node detected (%lu). Unknown type."), blk);
	goto error_free_node;
    }
	
    /* This callback function may be using to perform some checks */
    if (before_node_func && !(call_result = before_node_func(node, data)))
	goto error_free_node;
	
    if (!(call_result = node_func(node, data)))
	goto error_free_node;
	
    if (is_internal_node(node)) {
	uint32_t i;
		
	for (i = 0; i <= get_node_nritems(get_node_head(node)); i++) {
	    blk_t chld_blk = get_dc_child_blocknr(get_node_disk_child(node, i));
			
	    if (!(call_result = reiserfs_tree_node_traverse(tree, chld_blk + tree->offset,
		    data, before_node_func, node_func, chld_func, after_node_func)))
		goto error_free_node;
			
	    if (chld_func && !chld_func(node, i, call_result, data)) 
		goto error_free_node;
	}
    }

    /* This callback function may be using to save changed node */
    if (after_node_func && !(call_result = after_node_func(node, data)))
	goto error_free_node;

    reiserfs_block_free(node);
    return call_result;
	
error_free_node:
    reiserfs_block_free(node);    
error:
    return call_result;
}

long reiserfs_tree_simple_traverse(reiserfs_tree_t *tree, void *data,
    reiserfs_node_func_t node_func)
{
    if (reiserfs_tree_get_root(tree) < 2)
	return 1;
	
    return reiserfs_tree_node_traverse(tree, reiserfs_tree_get_root(tree) + tree->offset, 
	data, NULL, node_func, NULL, NULL);
}

long reiserfs_tree_traverse(reiserfs_tree_t *tree, void *data,
    reiserfs_edge_traverse_func_t before_node_func, reiserfs_node_func_t node_func, 
    reiserfs_chld_func_t chld_func, reiserfs_edge_traverse_func_t after_node_func)
{
    if (reiserfs_tree_get_height(tree) < 2)
	return 1;
	
    return reiserfs_tree_node_traverse(tree, reiserfs_tree_get_root(tree) + tree->offset, 
    	data, before_node_func, node_func, chld_func, after_node_func);
}

void reiserfs_tree_set_offset(reiserfs_tree_t *tree, long offset) {
    ASSERT(tree != NULL, return);
	
    if ((count_t)labs(offset) > dal_len(tree->fs->dal)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Invalid tree offset (%lu) has been detected."), offset);
	return;
    }
    
    tree->offset = -offset;
}

long reiserfs_tree_offset(reiserfs_tree_t *tree) {
    ASSERT(tree != NULL, return 0);
    return tree->offset;
}

void reiserfs_tree_free(reiserfs_tree_t *tree) {
    if (!tree) return;
    	libreiserfs_free(tree);
}
