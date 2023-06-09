/*
    dir.h -- reiserfs dir access code
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifndef DIR_H
#define DIR_H

#include "path.h"
#include "node.h"

struct reiserfs_de_head {
    uint32_t de_offset;
    uint32_t de_dirid;
    uint32_t de_objid;
    uint16_t de_location;
    uint16_t de_state;
} __attribute__ ((__packed__));

typedef struct reiserfs_de_head reiserfs_de_head_t;

#define EMPTY_DIR_V2_SIZE \
    (DE_SIZE * 2 + ROUND_UP(strlen (".")) + \
    ROUND_UP(strlen ("..")))

#define EMPTY_DIR_V1_SIZE		(DE_SIZE * 2 + 3)

#define get_de_offset(de)		get_le32(de, de_offset)
#define set_de_offset(de, val)		set_le32(de, de_offset, val)

#define get_de_dirid(de)		get_le32(de, de_dirid)
#define set_de_dirid(de, val)		set_le32(de, de_dirid, val)

#define get_de_objid(de)		get_le32(de, de_objid)
#define set_de_objid(de, val)		set_le32(de, de_objid, val)

#define get_de_location(de)		get_le16(de, de_location)
#define set_de_location(de, val)	set_le16(de, de_location, val)

#define get_de_state(de)		get_le16(de, de_state)
#define set_de_state(de, val)		set_le16(de, de_state, val)

#define DE_VISIBLE 			2
	
#define mark_de_visible(de) \
	reiserfs_tools_set_bit(DE_VISIBLE, &((de)->de_state))
	
#define is_de_visible(de) \
	reiserfs_tools_test_bit(DE_VISIBLE, &((de)->de_state))
	
#define MAX_NAME_LEN(blocksize) \
	(blocksize - NDHD_SIZE - IH_SIZE - DE_SIZE)

struct reiserfs_dir {
    reiserfs_object_t *entity;
	
    int32_t local;
    uint32_t offset;
};

typedef struct reiserfs_dir reiserfs_dir_t;

struct reiserfs_dir_entry {
    reiserfs_de_head_t de;
    char de_name[MAX_NAME_LEN(DEFAULT_BLOCK_SIZE)];
};

typedef struct reiserfs_dir_entry reiserfs_dir_entry_t;

extern reiserfs_dir_t *reiserfs_dir_open(reiserfs_fs_t *fs, 
    const char *name);

extern void reiserfs_dir_close(reiserfs_dir_t *dir);
extern int reiserfs_dir_rewind(reiserfs_dir_t *dir);
extern int reiserfs_dir_seek(reiserfs_dir_t *dir, uint32_t offset);

extern uint32_t reiserfs_dir_offset(reiserfs_dir_t *dir);

extern int reiserfs_dir_read(reiserfs_dir_t *dir, 
    reiserfs_dir_entry_t *entry);

extern int reiserfs_dir_find_key(reiserfs_dir_t *dir, uint32_t entry_hash, 
    struct key *key);

extern int reiserfs_dir_stat(reiserfs_dir_t *dir, struct stat *stat);

extern int reiserfs_dir_entry_hidden(reiserfs_dir_entry_t *entry);

#endif

