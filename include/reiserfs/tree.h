/*
    tree.h -- reiserfs balanced tree implementation
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifndef TREE_H
#define TREE_H

#include <dal/dal.h>

#include "key.h"
#include "block.h"
#include "object.h"
#include "path.h"
#include "filesystem.h"

/* Key for the tree root */
#define ROOT_DIR_ID 	1
#define ROOT_OBJ_ID 	2

/* Tree levels */
#define FREE_LEVEL 		0
#define LEAF_LEVEL 		1

#define MAX_HEIGHT 		5

typedef long (*reiserfs_node_func_t)(reiserfs_block_t *node, void *data);

typedef long (*reiserfs_chld_func_t)(reiserfs_block_t *node, uint32_t chld, 
    long result, void *data);

typedef long (*reiserfs_edge_traverse_func_t)(reiserfs_block_t *node, void *data);    

/* Tree functions */
extern reiserfs_tree_t *reiserfs_tree_create(reiserfs_fs_t *fs);
extern reiserfs_tree_t *reiserfs_tree_open(reiserfs_fs_t *fs);

extern reiserfs_path_node_t *reiserfs_tree_lookup_internal(reiserfs_tree_t *tree, 
    blk_t from, reiserfs_comp_func_t comp_func, struct key *key, reiserfs_path_t *path);

extern reiserfs_path_node_t *reiserfs_tree_lookup_leaf(reiserfs_tree_t *tree, 
    blk_t from, reiserfs_comp_func_t comp_func, struct key *key, reiserfs_path_t *path);

extern long reiserfs_tree_traverse(reiserfs_tree_t *tree, void *data,
    reiserfs_edge_traverse_func_t before_node_func, reiserfs_node_func_t node_func,
    reiserfs_chld_func_t chld_func, reiserfs_edge_traverse_func_t after_node_func);

extern long reiserfs_tree_simple_traverse(reiserfs_tree_t *tree, void *data,
    reiserfs_node_func_t node_func);

extern void reiserfs_tree_set_offset(reiserfs_tree_t *tree, long offset);
extern long reiserfs_tree_get_offset(reiserfs_tree_t *tree);

extern void reiserfs_tree_free(reiserfs_tree_t *tree);

/* Tree utilities */
extern dal_t *reiserfs_tree_dal(reiserfs_tree_t *tree);

extern void reiserfs_tree_set_root(reiserfs_tree_t *tree, blk_t root);
extern blk_t reiserfs_tree_get_root(reiserfs_tree_t *tree);

extern void reiserfs_tree_set_height(reiserfs_tree_t *tree, uint32_t height);
extern uint32_t reiserfs_tree_get_height(reiserfs_tree_t *tree);

#endif

