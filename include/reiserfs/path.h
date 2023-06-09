/*
    path.h -- reiserfs tree path
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifndef PATH_H
#define PATH_H

#include "block.h"

struct reiserfs_path_node {
    struct reiserfs_path_node *parent;
	
    reiserfs_block_t *node;
    unsigned int pos;
};

typedef struct reiserfs_path_node reiserfs_path_node_t;

/* Tree path */
struct reiserfs_path {
    unsigned int length;
    unsigned int max_length;
    reiserfs_path_node_t **nodes;
};

typedef struct reiserfs_path reiserfs_path_t;

extern reiserfs_path_node_t *reiserfs_path_node_create(
    reiserfs_path_node_t *parent, reiserfs_block_t *node, 
    unsigned int pos);

extern void reiserfs_path_node_free(reiserfs_path_node_t *node);

extern reiserfs_path_node_t *reiserfs_path_at(reiserfs_path_t *path, 
    unsigned int pos);

extern reiserfs_path_node_t *reiserfs_path_last(reiserfs_path_t *path);
extern void *reiserfs_path_last_item(reiserfs_path_t *path);
extern reiserfs_path_node_t *reiserfs_path_first(reiserfs_path_t *path);

extern int reiserfs_path_insert(reiserfs_path_t *path, 
    unsigned int pos, reiserfs_path_node_t *node);

extern int reiserfs_path_remove(reiserfs_path_t *path, 
    unsigned int pos);

extern int reiserfs_path_inc(reiserfs_path_t *path, 
    reiserfs_path_node_t *node);

extern int reiserfs_path_dec(reiserfs_path_t *path);

extern reiserfs_path_t *reiserfs_path_create(unsigned int length);
extern void reiserfs_path_clear(reiserfs_path_t *path);
extern void reiserfs_path_free(reiserfs_path_t *path);
extern  reiserfs_path_node_t *reiserfs_path_pop(reiserfs_path_t *path);
extern int reiserfs_path_empty(reiserfs_path_t *path);

#endif

