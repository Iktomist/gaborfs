/*
    object.h -- reiserfs objects
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.

    Some parts of this code are from the original reiserfs code, as found in 
    reiserfsprogs and the linux kernel.
    Copyright (C) 1996-2002 Hans Reiser, see COPYING.NAMESYS for licensing and 
    copyright details.
*/

#ifndef OBJECT_H
#define OBJECT_H

#include "key.h"
#include "filesystem.h"
#include "tools.h"

struct reiserfs_item_head {
    struct key ih_key;
	
    union {
	uint16_t ih_free_space;
	uint16_t ih_entry_count;
    } __attribute__ ((__packed__)) u;
	
    uint16_t ih_item_len;
    uint16_t ih_item_location;
    uint16_t ih_format;
} __attribute__ ((__packed__));

typedef struct reiserfs_item_head reiserfs_item_head_t;

#define DE_SIZE 			(sizeof(reiserfs_de_head_t))
#define IH_SIZE 			(sizeof(reiserfs_item_head_t))

#define SD_V2_SIZE 			(sizeof(reiserfs_sd_v2_t))
#define SD_V1_SIZE 			(sizeof(reiserfs_sd_v1_t))

#define is_indirect_ih(ih) 		is_indirect_key(&((ih)->ih_key))
#define is_direct_ih(ih) 		is_direct_key(&((ih)->ih_key))
#define is_direntry_ih(ih) 		is_direntry_key(&((ih)->ih_key))
#define is_stat_data_ih(ih) 		is_stat_data_key(&((ih)->ih_key))

#define get_ih_entry_count(ih) 		get_le16(ih, u.ih_entry_count)
#define set_ih_entry_count(ih, val)	set_le16(ih, u.ih_entry_count, val)

#define get_ih_free_space(ih) 		get_le16(ih, u.ih_free_space)
#define set_ih_free_space(ih, val) 	set_le16(ih, u.ih_free_space, 0)

#define get_ih_item_len(ih) 		get_le16(ih, ih_item_len)
#define set_ih_item_len(ih, val)	set_le16(ih, ih_item_len, val)

#define get_ih_item_location(ih) 	get_le16 (ih, ih_item_location)
#define set_ih_item_location(ih, val)	set_le16 (ih, ih_item_location, val)

#define get_ih_item_format(ih) 		get_le16(ih, ih_format)
#define set_ih_item_format(ih, val)	set_le16(ih, ih_format, val)

#define get_ih_item_head(bh, pos) \
	((reiserfs_item_head_t *)((bh)->data + NDHD_SIZE) + (pos))
	
#define get_ih_item_body(bh, ih) \
	((bh)->data + get_ih_item_location(ih))
	
#define get_ih_unfm_nr(ih) 		(get_ih_item_len(ih) / sizeof(uint32_t))

#define ITEM_FORMAT_1 			0
#define ITEM_FORMAT_2 			1

struct reiserfs_sd_v1 {
    uint16_t sd_mode;
    uint16_t sd_nlink;
    uint16_t sd_uid;
    uint16_t sd_gid;
    uint32_t sd_size;
    uint32_t sd_atime;
    uint32_t sd_mtime;
    uint32_t sd_ctime;
	
    union {
	uint32_t sd_rdev;
	uint32_t sd_blocks;
    } __attribute__ ((__packed__)) u;
    uint32_t sd_first_direct_byte;
} __attribute__ ((__packed__));

typedef struct reiserfs_sd_v1 reiserfs_sd_v1_t;

struct reiserfs_sd_v2 {
    uint16_t sd_mode;
    uint16_t sd_attrs;
    uint32_t sd_nlink;
    uint64_t sd_size;
    uint32_t sd_uid;
    uint32_t sd_gid;
    uint32_t sd_atime;
    uint32_t sd_mtime;
    uint32_t sd_ctime;
    uint32_t sd_blocks;
    union {
	uint32_t sd_rdev;
    } __attribute__ ((__packed__)) u;
} __attribute__ ((__packed__));

typedef struct reiserfs_sd_v2 reiserfs_sd_v2_t;

#define get_sd_v1_mode(sd)             	get_le16(sd, sd_mode)
#define set_sd_v1_mode(sd, val)         set_le16(sd, sd_mode, val)
#define get_sd_v1_nlink(sd)             get_le16(sd, sd_nlink)
#define set_sd_v1_nlink(sd, val)        set_le16(sd, sd_nlink, val)
#define get_sd_v1_uid(sd)         		get_le16(sd, sd_uid)
#define set_sd_v1_uid(sd, val)          set_le16(sd, sd_uid, val)
#define get_sd_v1_gid(sd)				get_le16(sd, sd_gid)
#define set_sd_v1_gid(sd, val)          set_le16(sd, sd_gid, val)
#define get_sd_v1_size(sd)           	get_le32(sd, sd_size)
#define set_sd_v1_size(sd, val)         set_le32(sd, sd_size, val)
#define get_sd_v1_atime(sd)             get_le32(sd, sd_atime)
#define set_sd_v1_atime(sd, val)        set_le32(sd, sd_atime, val)
#define get_sd_v1_mtime(sd)             get_le32(sd, sd_mtime)
#define set_sd_v1_mtime(sd, val)        set_le32(sd, sd_mtime, val)
#define get_sd_v1_ctime(sd)             get_le32(sd, sd_ctime)
#define set_sd_v1_ctime(sd, val)        set_le32(sd, sd_ctime, val)
#define get_sd_v1_blocks(sd)            get_le32(sd, u.sd_blocks)
#define set_sd_v1_blocks(sd, val)       set_le32(sd, u.sd_blocks, val)
#define get_sd_v1_rdev(sd)            	get_le32(sd, u.sd_rdev)
#define set_sd_v1_rdev(sd, val)         set_le32(sd, u.sd_rdev, val)
#define get_sd_v1_direct_byte(sd) 		get_le32(sd, sd_first_direct_byte)
#define set_sd_v1_direct_byte(sd, val) 	set_le32(sd, sd_first_direct_byte, val)

#define get_sd_v2_mode(sd)              get_le16(sd, sd_mode)
#define set_sd_v2_mode(sd, val)         set_le16(sd, sd_mode, val)
#define get_sd_v2_sd_attrs(sd)          get_le16(sd, sd_attrs)
#define set_sd_v2_sd_attrs(sd, val)     set_le16(sd, sd_attrs, val)
#define get_sd_v2_nlink(sd)             get_le32(sd, sd_nlink)
#define set_sd_v2_nlink(sd, val)        set_le32(sd, sd_nlink, val)
#define get_sd_v2_size(sd)              get_le64(sd, sd_size)
#define set_sd_v2_size(sd, val)         set_le64(sd, sd_size, val)
#define get_sd_v2_uid(sd)               get_le32(sd, sd_uid)
#define set_sd_v2_uid(sd, val)          set_le32(sd, sd_uid, val)
#define get_sd_v2_gid(sd)               get_le32(sd, sd_gid)
#define set_sd_v2_gid(sd, val)          set_le32(sd, sd_gid, val)
#define get_sd_v2_atime(sd)             get_le32(sd, sd_atime)
#define set_sd_v2_atime(sd, val)        set_le32(sd, sd_atime, val)
#define get_sd_v2_mtime(sd)             get_le32(sd, sd_mtime)
#define set_sd_v2_mtime(sd, val)        set_le32(sd, sd_mtime, val)
#define get_sd_v2_ctime(sd)             get_le32(sd, sd_ctime)
#define set_sd_v2_ctime(sd, val)        set_le32(sd, sd_ctime, val)
#define get_sd_v2_blocks(sd)            get_le32(sd, sd_blocks)
#define set_sd_v2_blocks(sd, val)       set_le32(sd, sd_blocks, val)
#define get_sd_v2_rdev(sd)              get_le32(sd, u.sd_rdev)
#define set_sd_v2_rdev(sd, val)         set_le32(sd, u.sd_rdev, val)

#define SD_OFFSET 			0
#define DOT_OFFSET 			1
#define DOT_DOT_OFFSET 			2

#define PATH_SEPARATOR			'/'

extern int reiserfs_object_test(reiserfs_fs_t *fs, uint32_t objectid);
extern int reiserfs_object_use(reiserfs_fs_t *fs, uint32_t objectid);

extern int reiserfs_object_find_stat(reiserfs_object_t *object);

extern int reiserfs_object_find_entry(reiserfs_path_node_t *leaf, 
    uint32_t entry_hash, struct key *entry_key);

extern int reiserfs_object_find_path(reiserfs_object_t *object, 
    const char *name, struct key *dirkey, int as_link);

extern reiserfs_path_node_t *reiserfs_object_seek_by_offset(
    reiserfs_object_t *object, uint64_t offset, uint64_t type, 
    reiserfs_comp_func_t comp_func);

extern reiserfs_object_t *reiserfs_object_create(reiserfs_fs_t *fs, 
    const char *name, int as_link);

extern void reiserfs_object_free(reiserfs_object_t *object);

extern int reiserfs_object_is_reg(reiserfs_object_t *object);
extern int reiserfs_object_is_dir(reiserfs_object_t *object);
extern int reiserfs_object_is_lnk(reiserfs_object_t *object);

#endif

