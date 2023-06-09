/*
    bitmap.h -- bitmap functions
    Copyright (C) 2001, 2002 Yury Umanets.
*/

#ifndef BITMAP_H
#define BITMAP_H

#include <dal/dal.h>
#include "filesystem.h"

typedef int (reiserfs_bitmap_pipe_func_t)(dal_t *, blk_t, char *, uint32_t, void *);

extern void reiserfs_bitmap_use_block(reiserfs_bitmap_t *bitmap, blk_t blk);
extern void reiserfs_bitmap_unuse_block(reiserfs_bitmap_t *bitmap, blk_t blk);
extern int reiserfs_bitmap_test_block(reiserfs_bitmap_t *bitmap, blk_t blk);
extern blk_t reiserfs_bitmap_find_free(reiserfs_bitmap_t *bitmap, blk_t start);

extern blk_t reiserfs_bitmap_calc_used(reiserfs_bitmap_t *bitmap);
extern blk_t reiserfs_bitmap_calc_unused(reiserfs_bitmap_t *bitmap);

extern blk_t reiserfs_bitmap_used(reiserfs_bitmap_t *bitmap);
extern blk_t reiserfs_bitmap_unused(reiserfs_bitmap_t *bitmap);

extern blk_t reiserfs_bitmap_calc_used_in_area(reiserfs_bitmap_t *bitmap, 
    blk_t start, blk_t end);

extern blk_t reiserfs_bitmap_calc_unused_in_area(reiserfs_bitmap_t *bitmap, 
    blk_t start, blk_t end);

extern int reiserfs_bitmap_check(reiserfs_bitmap_t *bitmap);

extern reiserfs_bitmap_t *reiserfs_bitmap_alloc(count_t len);

extern reiserfs_bitmap_t *reiserfs_bitmap_open(reiserfs_fs_t *fs, 
    blk_t start, count_t len);

extern reiserfs_bitmap_t *reiserfs_bitmap_create(reiserfs_fs_t *fs, 
    blk_t start, count_t len);

extern int reiserfs_bitmap_resize(reiserfs_bitmap_t *bitmap, 
    long start, long end);

extern blk_t reiserfs_bitmap_copy(reiserfs_bitmap_t *dest_bitmap, 
    reiserfs_bitmap_t *src_bitmap, count_t len);

extern reiserfs_bitmap_t *reiserfs_bitmap_clone(reiserfs_bitmap_t *bitmap);
extern int reiserfs_bitmap_sync(reiserfs_bitmap_t *bitmap);
extern void reiserfs_bitmap_close(reiserfs_bitmap_t *bitmap);

extern reiserfs_bitmap_t *reiserfs_bitmap_reopen(reiserfs_bitmap_t *bitmap);

extern int reiserfs_bitmap_pipe(reiserfs_bitmap_t *bitmap, 
    reiserfs_bitmap_pipe_func_t *pipe_func, void *data);

extern char *reiserfs_bitmap_map(reiserfs_bitmap_t *bitmap);

#endif

