/*
    block.c -- block functions
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <reiserfs/reiserfs.h>
#include <reiserfs/debug.h>

#include <dal/dal.h>

#if ENABLE_NLS
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif

reiserfs_block_t *reiserfs_block_alloc(dal_t *dal, blk_t blk, char c) {
    reiserfs_block_t *block;
	
    ASSERT(dal != NULL, return NULL);
	
    if (!(block = (reiserfs_block_t *)libreiserfs_calloc(sizeof(*block), 0)))
	return NULL;
	
    if (!(block->data = libreiserfs_calloc(dal_get_blocksize(dal), c)))
	goto error_free_block;
	
    block->offset = (uint64_t)blk * (uint64_t)dal_get_blocksize(dal);
    block->dal = dal;

    return block;
	
error_free_block:
    libreiserfs_free(block);
error:
    return NULL;
}

reiserfs_block_t *reiserfs_block_alloc_with_copy(dal_t *dal, blk_t blk, void *data) {
    reiserfs_block_t *block;
	
    if (!(block = reiserfs_block_alloc(dal, blk, 0)))
    	return NULL;
	
    if (data)
    	memcpy(block->data, data, dal_get_blocksize(dal));
	
    return block;
}

reiserfs_block_t *reiserfs_block_realloc(reiserfs_block_t *block, blk_t blk) {
    ASSERT(block != NULL, return NULL);

    if (!libreiserfs_realloc((void **)&block->data, dal_get_blocksize(block->dal)))
    	return NULL;
		
    block->offset = (uint64_t)blk * (uint64_t)dal_get_blocksize(block->dal);

    return block;
}

reiserfs_block_t *reiserfs_block_read(dal_t *dal, blk_t blk) {
    reiserfs_block_t *block;
	
    ASSERT(dal != NULL, return NULL);

    if (blk >= dal_len(dal))
	return NULL;
	
    if (!(block = reiserfs_block_alloc(dal, blk, 0)))
	return NULL;
	
    if (!dal_read(dal, block->data, blk, 1)) {
	reiserfs_block_free(block);
	return NULL;
    }
    return block;
}

int reiserfs_block_write(dal_t *dal, reiserfs_block_t *block) {

    ASSERT(block != NULL, return 0);
    ASSERT(dal != NULL, return 0);
	
    if (!dal_write(dal, block->data, reiserfs_block_get_nr(block), 1))
	return 0;
	
    return 1;
}

blk_t reiserfs_block_get_nr(reiserfs_block_t *block) {
    ASSERT(block != NULL, return 0);
    return (blk_t)(block->offset / dal_get_blocksize(block->dal));
}

void reiserfs_block_set_nr(reiserfs_block_t *block, blk_t blk) {
    ASSERT(block != NULL, return);
    block->offset = (uint64_t)blk * (uint64_t)dal_get_blocksize(block->dal);
}

dal_t *reiserfs_block_dal(reiserfs_block_t *block) {
    ASSERT(block != NULL, return NULL);
    return block->dal;
}

void reiserfs_block_set_dal(reiserfs_block_t *block, dal_t *dal) {
    ASSERT(block != NULL, return);
    ASSERT(dal != NULL, return);
	
    block->dal = dal;
}

int reiserfs_block_dirty(reiserfs_block_t *block) {
    ASSERT(block != NULL, return 0);
    return block->dirty;
}

void reiserfs_block_mark_dirty(reiserfs_block_t *block) {
    ASSERT(block != NULL, return);
    block->dirty = 1;
}

void reiserfs_block_mark_clean(reiserfs_block_t *block) {
    ASSERT(block != NULL, return);
    block->dirty = 0;
}

void reiserfs_block_free(reiserfs_block_t *block) {
    ASSERT(block != NULL, return);
	
    if (block->data)
    	libreiserfs_free(block->data);
	
    libreiserfs_free(block);
}

