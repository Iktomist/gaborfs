/*
    file.c -- reiserfs file access code
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <fcntl.h>

#include <reiserfs/reiserfs.h>
#include <reiserfs/debug.h>

#define N_(String) (String)
#if ENABLE_NLS
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif

static reiserfs_file_t *reiserfs_file_open_as(reiserfs_fs_t *fs, const char *name, 
    int mode, int as_link)
{
    reiserfs_file_t *file;
	
    ASSERT(fs != NULL, return NULL);
    ASSERT(name != NULL, return NULL);

    if (dal_flags(fs->dal) & O_RDONLY && mode & O_RDWR) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Can't open file for write on read only file system."));
	return NULL;    
    }

    if (!(file = libreiserfs_calloc(sizeof(*file), 0)))
	return NULL;

    if (!(file->entity = reiserfs_object_create(fs, name, as_link)))
	goto error_free_file;

    if (!reiserfs_object_is_reg(file->entity) && 
	!reiserfs_object_is_lnk(file->entity)) 
    {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Sorry, %s isn't a regular file or link to file."), name);
	goto error_free_entity;
    }
	
    file->size = file->entity->stat.st_size;

    if (!reiserfs_file_rewind(file))
	goto error_free_entity;
	    
    return file;

error_free_entity:
    reiserfs_object_free(file->entity);
error_free_file:
    libreiserfs_free(file);
error:
    return NULL;
}    

reiserfs_file_t *reiserfs_file_open(reiserfs_fs_t *fs, const char *name, int mode) {
    return reiserfs_file_open_as(fs, name, mode, 0);
}

reiserfs_file_t *reiserfs_link_open(reiserfs_fs_t *fs, const char *name, int mode) {
    return reiserfs_file_open_as(fs, name, mode, 1);
}

void reiserfs_file_close(reiserfs_file_t *file) {
    ASSERT(file != NULL, return);
    ASSERT(file->entity != NULL, return);

    reiserfs_object_free(file->entity);
    libreiserfs_free(file);
}

static int reiserfs_file_read_direct(reiserfs_file_t *file, void *buffer, 
    uint64_t size)
{
    uint32_t chunk, offset;
    reiserfs_path_node_t *leaf;
    reiserfs_item_head_t *item;
	
    leaf = reiserfs_path_last(file->entity->path);
    item = get_ih_item_head(leaf->node, leaf->pos);
	
    if ((chunk = get_ih_item_len(item) - file->offset_dt) == 0)
	return 1;
	
    if (chunk > size) chunk = size;
	
    memcpy(buffer, get_ih_item_body(leaf->node, item) + file->offset_dt, chunk);
	
    file->offset += chunk;
    file->offset_dt += chunk;
	
    return 1;
}

static uint64_t reiserfs_file_read_indirect(reiserfs_file_t *file, void *buffer, 
    uint64_t size)
{
    uint64_t readed = 0;
    uint32_t *blocks, offset, chunk;
	
    reiserfs_path_node_t *leaf;
    reiserfs_item_head_t *item;
	
    leaf = reiserfs_path_last(file->entity->path);
    item = get_ih_item_head(leaf->node, leaf->pos);
	
    blocks = (uint32_t *)get_ih_item_body(leaf->node, item);
	
    for (; file->offset_it < get_ih_unfm_nr(item) && readed < size; file->offset_it++) {
	reiserfs_block_t *block;
	
	if (blocks[file->offset_it] == 0) 
	    continue;
	
	if (!(block = reiserfs_block_read(file->entity->fs->dal, 
	    blocks[file->offset_it]))) 
	{
	    reiserfs_block_reading_failed(blocks[file->offset_it], 
		dal_error(file->entity->fs->dal), return 0);
	}
	    
	offset = file->offset % reiserfs_fs_block_size(file->entity->fs);
	chunk = reiserfs_fs_block_size(file->entity->fs) - offset;

	if (chunk > size - readed)
	    chunk = size - readed;
	    
	memcpy(buffer, block->data + offset, chunk);
	    
	reiserfs_block_free(block);
	
	buffer += chunk;
	readed += chunk;
	file->offset += chunk;
    }
    return 1;
}    

static int reiserfs_file_read_item(reiserfs_file_t *file, void *buffer, 
    uint64_t size)
{
    int result = 0;
    reiserfs_item_head_t *item = reiserfs_path_last_item(file->entity->path);

    if (reiserfs_key_type(&item->ih_key) == KEY_TYPE_DT)
	result = reiserfs_file_read_direct(file, buffer, size);
    else
	result = reiserfs_file_read_indirect(file, buffer, size);
	
    return result;	    
}

uint64_t reiserfs_file_read(reiserfs_file_t *file, void *buffer, uint64_t size) {
    uint64_t readed = 0, offset;
	
    ASSERT(file != NULL, return 0);
    ASSERT(buffer != NULL, return 0);
	
    if (file->offset >= file->size)
	return readed;
	
    offset = file->offset;
    while (reiserfs_file_seek(file, file->offset)) {
	
	if (!reiserfs_file_read_item(file, buffer + readed, size - readed))
	    return readed;
	
	readed += file->offset - offset;
	offset = file->offset;
    }
	
    return readed;
}

uint64_t reiserfs_file_size(reiserfs_file_t *file) {
    ASSERT(file != NULL, return 0);
    return file->size;
}

uint64_t reiserfs_file_offset(reiserfs_file_t *file) {
    ASSERT(file != NULL, return 0);
    return file->offset;
}

uint32_t reiserfs_file_inode(reiserfs_file_t *file) {
    ASSERT(file != NULL, return 0);
    return get_key_objid(&file->entity->key);
}

int reiserfs_file_rewind(reiserfs_file_t *file) {
    uint32_t key_type;
	
    ASSERT(file != NULL, return 0);
	
    key_type = (file->size > 
	MAX_DIRECT_ITEM_LEN(reiserfs_fs_block_size(file->entity->fs)) ? 
	KEY_TYPE_IT : KEY_TYPE_DT);
	
    if (!reiserfs_object_seek_by_offset(file->entity, 1, key_type, 
	reiserfs_key_comp_four_components))
    {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL,
	    _("Couldn't find first file block."));
	return 0;
    }
	
    file->offset = 0;
    file->offset_dt = 0;
    file->offset_it = 0;
    return 1;
}

int reiserfs_file_stat(reiserfs_file_t *file, struct stat *stat) {
    ASSERT(file != NULL, return 0);
    ASSERT(stat != NULL, return 0);
	
    memcpy(stat, &file->entity->stat, sizeof(*stat));
    return 1;
}

int reiserfs_file_seek(reiserfs_file_t *file, uint64_t offset) {
    uint64_t delta = 0;
    uint32_t block_pos;

    reiserfs_path_node_t *leaf;
    reiserfs_item_head_t *item;
	
    ASSERT(file != NULL, return 0);
    if (offset >= file->size)
	return 0;
	
    item = reiserfs_path_last_item(file->entity->path);
	
    if (file->offset_it >= get_ih_unfm_nr(item))
	return 0;

    if (!(leaf = reiserfs_object_seek_by_offset(file->entity, offset + 1, 0, 
	reiserfs_key_comp_three_components)))
    {
	leaf = reiserfs_path_last(file->entity->path);
	leaf->pos--;
    }

    item = reiserfs_path_last_item(file->entity->path);

    if (reiserfs_key_comp_two_components(&item->ih_key, &file->entity->key) != 0)
	return 0;
    
    file->offset = offset;
    delta = (offset > (uint64_t)reiserfs_key_offset(&item->ih_key) ?
	offset - reiserfs_key_offset(&item->ih_key) : 0);
	
    file->offset_it = (uint32_t)(delta / reiserfs_fs_block_size(file->entity->fs));
    file->offset_dt = delta;
	
    return 1;
}

