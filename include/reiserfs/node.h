/*
    node.h -- reiserfs b*tree node functions
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifndef NODE_H
#define NODE_H

#if defined(__sparc__) || defined(__sparcv9)
#  include <sys/int_types.h>
#elif defined(__freebsd__)
#  include <inttypes.h>
#else
#  include <stdint.h>
#endif

#define MAX_CHILD_SIZE(blocksize) 		((blocksize) - NDHD_SIZE)
#define MAX_FREE_SPACE(blocksize) 		MAX_CHILD_SIZE(blocksize)

#define is_leaf_node(node) \
    (get_node_level(get_node_head(node)) == LEAF_LEVEL)
	
#define is_internal_node(node) \
    ((get_node_level(get_node_head(node)) > LEAF_LEVEL) && \
    (get_node_level(get_node_head(node)) <= MAX_HEIGHT))

struct reiserfs_node_head {       
    uint16_t nh_level;
    uint16_t nh_nritems;
    uint16_t nh_free_space;
    uint16_t nh_reserved[9];
};

typedef struct reiserfs_node_head reiserfs_node_head_t;

#define NDHD_SIZE 				(sizeof(reiserfs_node_head_t))

#define get_node_head(bh)			((reiserfs_node_head_t *)((bh)->data))

#define get_node_level(node) 			get_le16(node, nh_level)
#define set_node_level(node, val) 		set_le16(node, nh_level, val)

#define get_node_nritems(node)			get_le16(node, nh_nritems)
#define set_node_nritems(node, val)		set_le16(node, nh_nritems, val)

#define get_node_free_space(node) 		get_le16(node, nh_free_space)
#define set_node_free_space(node, val)		set_le16(node, nh_free_space, val)

#define get_node_disk_child(bh, pos) \
	((reiserfs_disk_child_t *)((bh)->data + NDHD_SIZE + \
	get_node_nritems((reiserfs_node_head_t *)(bh)->data) * \
	FULL_KEY_SIZE + DC_SIZE * (pos)))
	
struct reiserfs_disk_child {
    uint32_t dc_blocknr;
    uint16_t dc_size;
    uint16_t dc_reserved;
};

typedef struct reiserfs_disk_child reiserfs_disk_child_t;

#define DC_SIZE					(sizeof(reiserfs_disk_child_t))

#define get_dc_child_blocknr(dc)		get_le32(dc, dc_blocknr)
#define set_dc_child_blocknr(dc, val)		set_le32(dc, dc_blocknr, val)

#define get_dc_child_size(dc) 			get_le16(dc, dc_size)
#define set_dc_child_size(dc, val) 		set_le16(dc, dc_size, val)

#endif

