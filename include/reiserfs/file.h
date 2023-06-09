/*
    file.h -- reiserfs file access code
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifndef FILE_H
#define FILE_H

#include "filesystem.h"
#include "path.h"

#define MAX_DIRECT_ITEM_LEN(blocksize) \
    ((blocksize) - NDHD_SIZE - 2*IH_SIZE \
    - SD_V1_SIZE - sizeof(uint32_t))

struct reiserfs_file {
    reiserfs_object_t *entity;
	
    uint32_t offset_it;
    uint64_t offset_dt;
	
    uint64_t size;
    uint64_t offset;
};

typedef struct reiserfs_file reiserfs_file_t;

extern reiserfs_file_t *reiserfs_file_open(reiserfs_fs_t *fs, 
    const char *name, int mode);

extern reiserfs_file_t *reiserfs_link_open(reiserfs_fs_t *fs, 
    const char *name, int mode);

extern void reiserfs_file_close(reiserfs_file_t *file);

extern uint64_t reiserfs_file_read(reiserfs_file_t *file, 
    void *buffer, uint64_t size);

extern uint64_t reiserfs_file_size(reiserfs_file_t *file);
extern uint64_t reiserfs_file_offset(reiserfs_file_t *file);

extern int reiserfs_file_seek(reiserfs_file_t *file, 
    uint64_t offset);

extern int reiserfs_file_rewind(reiserfs_file_t *file);

extern int reiserfs_file_stat(reiserfs_file_t *file, 
    struct stat *stat);

extern uint32_t reiserfs_file_inode(reiserfs_file_t *file);

#endif

