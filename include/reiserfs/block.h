/*
    block.h -- block functions
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifndef BLOCK_H
#define BLOCK_H

#include <dal/dal.h>

#define reiserfs_block_reading_failed(blk, error, action) \
    do { \
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, \
	    _("Reading block %lu failed. %s."), blk, error); \
	    action; \
    } while (0)

#define reiserfs_block_writing_failed(blk, error, action) \
    do { \
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, \
	    _("Writing block %lu failed. %s."), blk, error); \
	action; \
    } while (0)

struct reiserfs_block {
    dal_t *dal;
	
    char *data;
    uint64_t offset;
	
    int dirty;
};

typedef struct reiserfs_block reiserfs_block_t;

extern reiserfs_block_t *reiserfs_block_alloc(dal_t *dal, blk_t blk, char c);
extern reiserfs_block_t *reiserfs_block_realloc(reiserfs_block_t *block, blk_t blk);

extern reiserfs_block_t *reiserfs_block_alloc_with_copy(dal_t *dal, blk_t blk,
    void *data);

extern dal_t *reiserfs_block_get_dal(reiserfs_block_t *block);
extern void reiserfs_block_set_dal(reiserfs_block_t *block, dal_t *dal);

extern blk_t reiserfs_block_get_nr(reiserfs_block_t *block);
extern void reiserfs_block_set_nr(reiserfs_block_t *block, blk_t blk);

extern reiserfs_block_t *reiserfs_block_read(dal_t *dal, blk_t blk);
extern int reiserfs_block_write(dal_t *dal, reiserfs_block_t *block);

extern int reiserfs_block_dirty(reiserfs_block_t *block);
extern void reiserfs_block_mark_dirty(reiserfs_block_t *block);
extern void reiserfs_block_mark_clean(reiserfs_block_t *block);

extern void reiserfs_block_free(reiserfs_block_t *block);

#endif

