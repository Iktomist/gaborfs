/*
    dir.c -- reiserfs dir access code
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <reiserfs/reiserfs.h>
#include <reiserfs/debug.h>

#if ENABLE_NLS
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif

int reiserfs_dir_stat(reiserfs_dir_t *dir, struct stat *stat) {
    ASSERT(dir != NULL, return 0);
    ASSERT(stat != NULL, return 0);
	
    memcpy(stat, &dir->entity->stat, sizeof(*stat));
	
    return 1;
}

reiserfs_dir_t *reiserfs_dir_open(reiserfs_fs_t *fs, const char *name) {
    reiserfs_dir_t *dir;

    ASSERT(fs != NULL, return NULL);
    ASSERT(name != NULL, return NULL);
	
    if (!(dir = libreiserfs_calloc(sizeof(*dir), 0)))
	goto error;
	
    if (!(dir->entity = reiserfs_object_create(fs, name, 0)))
	goto error_free_dir;
	
    if (!reiserfs_object_is_dir(dir->entity)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Sorry, %s isn't a directory."), name);
	goto error_free_entity;
    }
	
    if (!reiserfs_dir_rewind(dir))
	goto error_free_entity;
	
    return dir;

error_free_entity:
    reiserfs_object_free(dir->entity);
error_free_dir:
    libreiserfs_free(dir);
error:
    return NULL;
}

void reiserfs_dir_close(reiserfs_dir_t *dir) {
    ASSERT(dir != NULL, return);
    ASSERT(dir->entity != NULL, return);
	
    reiserfs_object_free(dir->entity);
    libreiserfs_free(dir);
}

int reiserfs_dir_rewind(reiserfs_dir_t *dir) {
    ASSERT(dir != NULL, return 0);
	
    if (!reiserfs_object_seek_by_offset(dir->entity, 
	DOT_OFFSET, KEY_TYPE_DR, reiserfs_key_comp_four_components)) 
    {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't find first directory block."));
	return 0;
    }
	
    dir->local = 0; 
    dir->offset = 0;
	
    return 1;
}

int reiserfs_dir_seek(reiserfs_dir_t *dir, uint32_t offset) {
    int direction;
    struct key key, *rkey;
    reiserfs_path_node_t *leaf;
    reiserfs_item_head_t *item;

    ASSERT(dir != NULL, return 0);
	
    if (offset < (uint32_t)labs(offset - dir->offset))
	reiserfs_dir_rewind(dir);
	
    direction = (offset > dir->offset);
	
    while (dir->offset != offset) {
	leaf = reiserfs_path_last(dir->entity->path);
	item = reiserfs_path_last_item(dir->entity->path);
		
	if ((direction && dir->local >= get_ih_entry_count(item)) || 
	    (!direction && dir->local < 0)) 
	{
	    if (!leaf->parent) break;
		
	    rkey = (struct key *)(leaf->parent->node->data + NDHD_SIZE) + 
		leaf->parent->pos - (direction ? 0 : 1);
		
	    if (reiserfs_key_comp_two_components(rkey, &dir->entity->key) ||
		   reiserfs_key_type(rkey) != KEY_TYPE_DR)
		break;
			
	    if (!reiserfs_object_seek_by_offset(dir->entity, reiserfs_key_offset(rkey), 
		    KEY_TYPE_DR, reiserfs_key_comp_four_components))
		break;
			
		item = reiserfs_path_last_item(dir->entity->path);
		dir->local = (direction ? 0 : get_ih_entry_count(item) - 1);
		direction ? dir->offset++ : dir->offset--;
	} else {
	    uint32_t internal_off;
			
	    if (direction) {
		internal_off = get_ih_entry_count(item) - dir->local;
		if (dir->offset + internal_off > offset) {
		    uint32_t diff = (dir->offset + internal_off) - offset;
		    internal_off = internal_off - diff;
		}
	    } else {
		internal_off = dir->local;
		if (dir->offset - internal_off < offset) {
		    uint32_t diff = offset - (dir->offset - internal_off);
		    internal_off = internal_off + diff;
		}
	    }

	    dir->local += (direction ? internal_off : -internal_off);
	    dir->offset += (direction ? internal_off : -internal_off);
	}
    }
	
    return (offset == dir->offset);
}

uint32_t reiserfs_dir_offset(reiserfs_dir_t *dir) {
    return dir->offset;
}

static uint32_t reiserfs_dir_entry_name_length(reiserfs_item_head_t *dir_item, 
    reiserfs_de_head_t *de, int pos_in_item) 
{
    if (pos_in_item) 
	return (get_de_location(de - 1) - get_de_location(de));

    return (get_ih_item_len(dir_item) - get_de_location(de));
}

static int reiserfs_dir_entry_create(reiserfs_item_head_t *dir_item,  
    reiserfs_de_head_t *de, int pos_in_item, reiserfs_dir_entry_t *entry)
{
    char *de_name;
    int de_name_len = 0;
	
    memcpy(&entry->de, de, sizeof(*de));
	
    de_name = ((char *)(de - pos_in_item) + get_de_location(de));
    memset(entry->de_name, 0, sizeof(entry->de_name));
	
    de_name_len = reiserfs_dir_entry_name_length(dir_item, de, pos_in_item);
    memcpy(entry->de_name, de_name, de_name_len);
    
    /* Copying the dirid and objectid too */
    memcpy(&entry->de, de, sizeof(*de));
	
    return 1;
}

static int reiserfs_dir_entry_read(reiserfs_dir_t *dir, reiserfs_dir_entry_t *entry) {
    reiserfs_de_head_t *de;
    reiserfs_item_head_t *item;
    reiserfs_path_node_t *leaf;

    ASSERT(dir != NULL, return 0);
	
    leaf = reiserfs_path_last(dir->entity->path);
    item = reiserfs_path_last_item(dir->entity->path);

    de = ((reiserfs_de_head_t *)get_ih_item_body(leaf->node, item)) + dir->local;
    reiserfs_dir_entry_create(item, de, dir->local, entry);

    dir->local++;
    dir->offset++;
    return 1;
}

int reiserfs_dir_read(reiserfs_dir_t *dir, reiserfs_dir_entry_t *entry) {
    struct key key;
    reiserfs_item_head_t *item;

    ASSERT(dir != NULL, return 0);
	
    if (!(item = reiserfs_path_last_item(dir->entity->path)))
	return 0;
	
    if (dir->local >= get_ih_entry_count(item)) {
	if (!reiserfs_dir_seek(dir, dir->offset + 1))
	   return 0;
    }	
	
    return reiserfs_dir_entry_read(dir, entry);
}

int reiserfs_dir_entry_hidden(reiserfs_dir_entry_t *entry) {
    return !is_de_visible(&entry->de);
}

