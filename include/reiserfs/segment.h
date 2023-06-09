/*
    segment.h -- filesystem geometry functions
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifndef SEGMENT_H
#define SEGMENT_H

#include <dal/dal.h>

#include "gauge.h"
#include "block.h"

struct reiserfs_segment {
    dal_t *dal;
    blk_t start, end;
};

typedef struct reiserfs_segment reiserfs_segment_t;

typedef int (*reiserfs_segment_func_t)(reiserfs_segment_t *, 
    reiserfs_block_t *, long, void *);

extern reiserfs_segment_t *reiserfs_segment_create(dal_t *dal, blk_t start, blk_t end);

extern int reiserfs_segment_init(reiserfs_segment_t *segment, 
    dal_t *dal, blk_t start, blk_t end);

extern void reiserfs_segment_free(reiserfs_segment_t *segment);    

extern int reiserfs_segment_fill(reiserfs_segment_t *segment, char c, 
    reiserfs_segment_func_t segment_func, void *data);

extern int reiserfs_segment_move(reiserfs_segment_t *dst_segment, 
    reiserfs_segment_t *src_segment, reiserfs_segment_func_t segment_func, void *data);

extern blk_t reiserfs_segment_relocate(reiserfs_fs_t *dst_fs, 
    reiserfs_segment_t *dst_segment, reiserfs_fs_t *src_fs, 
    reiserfs_segment_t *src_segment, int smart);

extern int reiserfs_segment_test_inside(reiserfs_segment_t *segment, blk_t blk);

extern int reiserfs_segment_test_overlap(reiserfs_segment_t *segment1, 
    reiserfs_segment_t *segment2);

extern blk_t reiserfs_segment_len(reiserfs_segment_t *segment);

#endif

