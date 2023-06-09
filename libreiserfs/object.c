/*
    object.c -- reiserfs files and directories access common code
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

/*
    Ext2/linux mode flags. We define them here so that we don't need to depend on 
    the OS's sys/stat.h, since we may be compiling on a non-Linux system.
*/
#define LINUX_S_IFMT	    00170000
#define LINUX_S_IFSOCK	    0140000
#define LINUX_S_IFLNK	    0120000
#define LINUX_S_IFREG	    0100000
#define LINUX_S_IFBLK	    0060000
#define LINUX_S_IFDIR	    0040000
#define LINUX_S_IFCHR	    0020000
#define LINUX_S_IFIFO	    0010000
#define LINUX_S_ISUID	    0004000
#define LINUX_S_ISGID	    0002000
#define LINUX_S_ISVTX	    0001000

#define LINUX_S_IRWXU	    00700
#define LINUX_S_IRUSR	    00400
#define LINUX_S_IWUSR	    00200
#define LINUX_S_IXUSR	    00100

#define LINUX_S_IRWXG	    00070
#define LINUX_S_IRGRP	    00040
#define LINUX_S_IWGRP	    00020
#define LINUX_S_IXGRP	    00010

#define LINUX_S_IRWXO	    00007
#define LINUX_S_IROTH	    00004
#define LINUX_S_IWOTH	    00002
#define LINUX_S_IXOTH	    00001

#define LINUX_S_ISLNK(m)    (((m) & LINUX_S_IFMT) == LINUX_S_IFLNK)
#define LINUX_S_ISREG(m)    (((m) & LINUX_S_IFMT) == LINUX_S_IFREG)
#define LINUX_S_ISDIR(m)    (((m) & LINUX_S_IFMT) == LINUX_S_IFDIR)
#define LINUX_S_ISCHR(m)    (((m) & LINUX_S_IFMT) == LINUX_S_IFCHR)
#define LINUX_S_ISBLK(m)    (((m) & LINUX_S_IFMT) == LINUX_S_IFBLK)
#define LINUX_S_ISFIFO(m)   (((m) & LINUX_S_IFMT) == LINUX_S_IFIFO)
#define LINUX_S_ISSOCK(m)   (((m) & LINUX_S_IFMT) == LINUX_S_IFSOCK)

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <unistd.h>

#if defined(__sparc__) || defined(__sparcv9)
#  include <reiserfs/strsep.h>
#endif

#include <reiserfs/reiserfs.h>
#include <reiserfs/debug.h>

#define N_(String) (String)
#if ENABLE_NLS
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif

reiserfs_path_node_t *reiserfs_object_seek_by_offset(reiserfs_object_t *object, 
    uint64_t offset, uint64_t type, reiserfs_comp_func_t comp_func)
{
    if (reiserfs_fs_format(object->fs) == FS_FORMAT_3_6) {
	set_key_v2_offset(&object->key, offset);
	set_key_v2_type(&object->key, type);
    } else {
	set_key_v1_offset(&object->key, (uint32_t)offset);
	set_key_v1_type(&object->key, reiserfs_key_type2uniq((uint32_t)type));
    }
	
    return reiserfs_tree_lookup_leaf(object->fs->tree, 
	reiserfs_tree_get_root(object->fs->tree), comp_func, &object->key, object->path);
}

int reiserfs_object_test(reiserfs_fs_t *fs, uint32_t objectid) {
    int i;
    uint32_t *objectids;
	
    ASSERT(fs != NULL, return 0);
    objectids = get_sb_objectid_map(fs->super);
  
    for (i = 0; i < get_sb_oid_cursize(fs->super); i += 2) {
	if (objectid == LE32_TO_CPU(objectids[i]))
	    return 1;
		
	if (objectid > LE32_TO_CPU(objectids[i]) &&	
		objectid < LE32_TO_CPU(objectids[i + 1]))
	    return 1;
		
	if (objectid < LE32_TO_CPU(objectids[i]))
	    break;
    }
    return 0;
}

int reiserfs_object_use(reiserfs_fs_t *fs, uint32_t objectid) {
    int i, cursize;
    uint32_t *objectids;

    ASSERT(fs != NULL, return 0);
	
    if (reiserfs_object_test(fs, objectid))
	return 1;

    objectids = get_sb_objectid_map(fs->super);
    cursize = get_sb_oid_cursize(fs->super);

    for (i = 0; i < cursize; i += 2) {
	if (objectid >= LE32_TO_CPU(objectids[i]) && 
		objectid < LE32_TO_CPU(objectids[i + 1]))
	    return 1;
	
	if (objectid + 1 == LE32_TO_CPU(objectids[i])) {
	    objectids[i] = CPU_TO_LE32(objectid);
	    goto mark_super;
	}
	
	if (objectid == LE32_TO_CPU(objectids[i + 1])) {
	    objectids[i + 1] = CPU_TO_LE32(LE32_TO_CPU(objectids[i + 1]) + 1);

	    if (i + 2 < cursize) {
		if (objectids[i + 1] == objectids[i + 2]) {
		    memmove(objectids + i + 1, objectids + i + 1 + 2, 
			(cursize - (i + 2 + 2 - 1)) * sizeof(uint32_t));
		    set_sb_oid_cursize(fs->super, cursize - 2);
		}
	    }
	    goto mark_super;
	}
	
	if (objectid < LE32_TO_CPU(objectids[i])) {
	    if (cursize == get_sb_oid_maxsize(fs->super)) {
		objectids[i] = CPU_TO_LE32(objectid);
		goto mark_super;
	    } else {
		memmove(objectids + i + 2, objectids + i, (cursize - i) * 
		    sizeof(uint32_t));
		set_sb_oid_cursize(fs->super, cursize + 2);
	    }
	    
	    objectids[i] = CPU_TO_LE32(objectid);
	    objectids[i + 1] = CPU_TO_LE32(objectid + 1);
	    goto mark_super;
	}
    }
	
    if (i < get_sb_oid_maxsize(fs->super)) {
	objectids[i] = CPU_TO_LE32(objectid);
	objectids[i + 1] = CPU_TO_LE32(objectid + 1);
	set_sb_oid_cursize(fs->super, cursize + 2);
    } else if (i == get_sb_oid_maxsize(fs->super))
	objectids[i - 1] = CPU_TO_LE32(objectid + 1);
    else
	return 0;
    
mark_super:
    reiserfs_fs_mark_super_dirty(fs);	
    return 1;	    
}

static void object_fill_stat(reiserfs_object_t *object, int format, void *sd) {
    uint32_t dev;
    reiserfs_sd_v1_t *sd_v1;
    reiserfs_sd_v2_t *sd_v2;

    memset(&object->stat, 0, sizeof(object->stat));
	
    object->stat.st_ino = get_key_objid(&object->key);
    object->stat.st_blksize = reiserfs_fs_block_size(object->fs);
	
    if (format == ITEM_FORMAT_1) {
	sd_v1 = (reiserfs_sd_v1_t *)sd;

	object->stat.st_mode = get_sd_v1_mode(sd_v1);
	object->stat.st_nlink = get_sd_v1_nlink(sd_v1);
	object->stat.st_uid = get_sd_v1_uid(sd_v1);
	object->stat.st_gid = get_sd_v1_gid(sd_v1);
	object->stat.st_rdev = get_sd_v1_rdev(sd_v1);
	object->stat.st_size = get_sd_v1_size(sd_v1);
	
#ifndef DJGPP
	object->stat.st_blocks = get_sd_v1_blocks(sd_v1);
#endif
	object->stat.st_atime = get_sd_v1_atime(sd_v1);
	object->stat.st_mtime = get_sd_v1_mtime(sd_v1);
	object->stat.st_ctime = get_sd_v1_ctime(sd_v1);
    } else {
	sd_v2 = (reiserfs_sd_v2_t *)sd;
		
	object->stat.st_mode = get_sd_v2_mode(sd_v2);
	object->stat.st_nlink = get_sd_v2_nlink(sd_v2);
	object->stat.st_uid = get_sd_v2_uid(sd_v2);
	object->stat.st_gid = get_sd_v2_gid(sd_v2);
	object->stat.st_rdev = get_sd_v2_rdev(sd_v2);
	object->stat.st_size = get_sd_v2_size(sd_v2);
	object->stat.st_atime = get_sd_v2_atime(sd_v2);
	object->stat.st_mtime = get_sd_v2_mtime(sd_v2);
	object->stat.st_ctime = get_sd_v2_ctime(sd_v2);
    }
}

int reiserfs_object_find_stat(reiserfs_object_t *object) {
    void *sd;
    reiserfs_path_node_t *leaf;
    reiserfs_item_head_t *item;
	
    if (!(leaf = reiserfs_object_seek_by_offset(object, SD_OFFSET, 
	KEY_TYPE_SD, reiserfs_key_comp_four_components)))
    {	
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't find stat data of object (%lu %lu)."), 
	    get_key_dirid(&object->key), get_key_objid(&object->key));
	return 0;
    }
	
    item = get_ih_item_head(leaf->node, leaf->pos);
    sd = get_ih_item_body(leaf->node, item);

    object_fill_stat(object, get_ih_item_format(item), sd);
	
    return 1;
}

static int reiserfs_object_link(reiserfs_path_node_t *leaf, char *link) {
    reiserfs_item_head_t *item;
    
    ASSERT(leaf != NULL, return 0);
    ASSERT(link != NULL, return 0);

    if (leaf->pos >= get_node_nritems(get_node_head(leaf->node)))
	return 0;
	
    item = get_ih_item_head(leaf->node, leaf->pos + 1);

    if (reiserfs_key_type(&item->ih_key) != KEY_TYPE_DT)
	return 0;
	
    memcpy(link, get_ih_item_body(leaf->node, item), get_ih_item_len(item));
	
    return 1;
}

int reiserfs_object_find_entry(reiserfs_path_node_t *leaf, uint32_t hash, 
    struct key *entry_key)
{
    uint32_t entry_pos;
    reiserfs_item_head_t *item;
	
    item = get_ih_item_head(leaf->node, leaf->pos);
	
    if (reiserfs_key_type(&item->ih_key) != KEY_TYPE_DR) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Invalid key type detected %d."), reiserfs_key_type(&item->ih_key));
	return 0;
    }
	
    if (reiserfs_tools_fast_search(&hash, get_ih_item_body(leaf->node, item), 
	get_ih_entry_count(item), DE_SIZE, reiserfs_tools_comp_generic, &entry_pos)) 
    {
	reiserfs_de_head_t *deh = ((reiserfs_de_head_t *)get_ih_item_body(leaf->node, 
	    item)) + entry_pos;
	set_key_dirid(entry_key, get_de_dirid(deh));
	set_key_objid(entry_key, get_de_objid(deh));
	return 1;
    }

    return 0;
}

int reiserfs_object_find_path(reiserfs_object_t *object, const char *name, 
    struct key *dirkey, int as_link) 
{
    int name_len;
    uint32_t hash;
    uint16_t *mode;
    char track[4096], path[4096];
    char *pointer = NULL, *dirname = NULL;
    char path_separator[2] = {PATH_SEPARATOR, '\0'}; 
	
    reiserfs_path_node_t *leaf;
	
    ASSERT(name != NULL, return 0);
	
    memset(path, 0, sizeof(path));
	
    name_len = (strlen(name) < sizeof(path) ? strlen(name) : sizeof(path) - 1);
    memcpy(path, name, name_len);
	
    memset(track, 0, sizeof(track));
	
    if (path[0] != '.' || path[0] == PATH_SEPARATOR)
	track[strlen(track)] = PATH_SEPARATOR;
	
    pointer = &path[0];
    while (1) {
	
	/* Looking for stat data */
	if (!(leaf = reiserfs_object_seek_by_offset(object, SD_OFFSET, 
	    KEY_TYPE_SD, reiserfs_key_comp_four_components)))
	{
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		_("Couldn't find stat data of directory %s."), track);
	    return 0;
	}
		
	/* Checking whether found item is a link. */
	mode = (uint16_t *)get_ih_item_body(leaf->node, get_ih_item_head(leaf->node, 
	    leaf->pos));
		
	if (!LINUX_S_ISLNK(LE16_TO_CPU(*mode)) && !LINUX_S_ISDIR(LE16_TO_CPU(*mode)) && 
	    !LINUX_S_ISREG(LE16_TO_CPU(*mode))) 
	{
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		_("%s has invalid object type."), track);
	    return 0;
	}
		
	if (LINUX_S_ISLNK(LE16_TO_CPU(*mode))) {
	    int is_terminator = dirname && 
		!strchr((dirname + strlen(dirname) + 1), PATH_SEPARATOR);
			
	    if (!as_link || !is_terminator) {
	    	char link[MAX_DIRECT_ITEM_LEN(reiserfs_fs_block_size(object->fs))];
		memset(link, 0, sizeof(link));
	    
		if (!reiserfs_object_link(leaf, link) || strlen(link) == 0)
		    return 0;
				
		set_key_dirid(&object->key, 
		    (link[0] == PATH_SEPARATOR ? ROOT_DIR_ID : get_key_dirid(dirkey)));
		    
		set_key_objid(&object->key, 
		    (link[0] == PATH_SEPARATOR ? ROOT_OBJ_ID : get_key_objid(dirkey)));
				
		if (!reiserfs_object_find_path(object, link, dirkey, 1)) {
		    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
			_("Couldn't follow link %s."), link);
		    return 0;
		}
	    }
	}

	set_key_dirid(dirkey, get_key_dirid(&object->key));
	set_key_objid(dirkey, get_key_objid(&object->key));

	if (!(dirname = strsep(&pointer, path_separator)))
	    break;
		
	if (!strlen(dirname))
	    continue;
		
	strncat(track, dirname, strlen(dirname));
		
	hash = reiserfs_fs_hash_value(object->fs, dirname);
		
	/* Finding directory item */	
	if (!(leaf = reiserfs_object_seek_by_offset(object, hash, 
	    KEY_TYPE_DR, reiserfs_key_comp_four_components)))
	{
	    leaf = reiserfs_path_last(object->path);
	    leaf->pos--;
	}

	/* Finding corresponding dir entry */
	if (!reiserfs_object_find_entry(leaf, hash, &object->key)) {
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		_("Couldn't find entry %s."), track);
	    return 0;
	}
		
	track[strlen(track)] = PATH_SEPARATOR;
    }
	
    return 1;
}

static void reiserfs_object_make_absolute_name(const char *name, char *buff, int size) {
    memset(buff, 0, size);
    if (name[0] != PATH_SEPARATOR) {
	getcwd(buff, size);
	buff[strlen(buff)] = PATH_SEPARATOR;
	memcpy(buff + strlen(buff), name, strlen(name));
    } else
	memcpy(buff, name, strlen(name));
}

reiserfs_object_t *reiserfs_object_create(reiserfs_fs_t *fs, 
    const char *name, int as_link) 
{
    struct key dirkey; 
    char absolute[4096];
    reiserfs_object_t *object;
	
    ASSERT(fs != NULL, return NULL);
    ASSERT(name != NULL, return NULL);
    ASSERT(strlen(name) > 0, return NULL);
	
    reiserfs_object_make_absolute_name(name, absolute, sizeof(absolute));
	
    if (!(object = libreiserfs_calloc(sizeof(*object), 0)))
	return NULL;
	
    if (!(object->path = reiserfs_path_create(MAX_HEIGHT)))
	goto error_free_object;
	
    object->fs = fs;
	
    reiserfs_key_form(&dirkey, ROOT_DIR_ID - 1, ROOT_OBJ_ID - 1, 
	SD_OFFSET, KEY_TYPE_SD, reiserfs_fs_format(fs));
	
    reiserfs_key_form(&object->key, ROOT_DIR_ID, ROOT_OBJ_ID, 
    	SD_OFFSET, KEY_TYPE_SD, reiserfs_fs_format(fs));
	
    if (!reiserfs_object_find_path(object, absolute, &dirkey, as_link))
	goto error_free_path;
	
    if (!reiserfs_object_find_stat(object))
	goto error_free_path;
	
    return object;
	
error_free_path:
    reiserfs_path_free(object->path);    
error_free_object:
    libreiserfs_free(object);
error:
    return NULL;    
}

int reiserfs_object_is_reg(reiserfs_object_t *object) {
    return LINUX_S_ISREG(object->stat.st_mode);
}

int reiserfs_object_is_dir(reiserfs_object_t *object) {
    return LINUX_S_ISDIR(object->stat.st_mode);
}

int reiserfs_object_is_lnk(reiserfs_object_t *object) {
    return LINUX_S_ISLNK(object->stat.st_mode);
}

void reiserfs_object_free(reiserfs_object_t *object) {
	
    ASSERT(object != NULL, return);
	
    reiserfs_path_free(object->path);
    libreiserfs_free(object);
}

