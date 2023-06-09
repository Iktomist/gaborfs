/*
    key.c -- reiserfs keys code
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
	
    Some parts of this code are from original reiserfs code as found in reiserfsprogs 
    and the linux kernel.
    Copyright 1996-2002 Hans Reiser, see COPYING.NAMESYS for licensing 
    and copyright details
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <reiserfs/reiserfs.h>

#define N_(String) (String)
#if ENABLE_NLS
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif

#define KEY_OFFSET_MASK 0xfffffffffffffffLL
#define KEY_TYPE_MASK 	0xf000000000000000LL

uint32_t reiserfs_key_uniq2type(uint32_t uniq) {
    switch (uniq) {
	case KEY_UNIQ_SD: return KEY_TYPE_SD;
	case KEY_UNIQ_IT: return KEY_TYPE_IT;
	case KEY_UNIQ_DT: return KEY_TYPE_DT;
	case KEY_UNIQ_DR: return KEY_TYPE_DR;
    }
    return KEY_TYPE_UN;
}

uint32_t reiserfs_key_type2uniq(uint32_t type) {
    switch (type) {
	case KEY_TYPE_SD: return KEY_UNIQ_SD;
	case KEY_TYPE_IT: return KEY_UNIQ_IT;
	case KEY_TYPE_DT: return KEY_UNIQ_DT;
	case KEY_TYPE_DR: return KEY_UNIQ_DR;
    } 
    return KEY_UNIQ_UN;
}

uint64_t get_key_v2_offset(const struct key *key) {
    uint64_t * p, tmp;

    p = (uint64_t *)(&(key->u.k_offset_v2));
    tmp = LE64_TO_CPU(*p);
    tmp &= KEY_OFFSET_MASK;
    tmp >>= 0;
    return tmp;
}

void set_key_v2_offset(struct key *key, uint64_t val) {
    uint64_t * p, tmp;

    p = (uint64_t *)(&(key->u.k_offset_v2));
    tmp = LE64_TO_CPU(*p);
    tmp &= ~KEY_OFFSET_MASK;
    tmp |= (val << 0);

    *p = CPU_TO_LE64 (tmp);
}

uint16_t get_key_v2_type(const struct key *key) {
    uint64_t * p, tmp;

    p = (uint64_t *)(&(key->u.k_offset_v2));
    tmp = LE64_TO_CPU (*p);
    tmp &= KEY_TYPE_MASK;
    tmp >>= 60;
    return (uint16_t)tmp;
}

void set_key_v2_type(struct key *key, uint64_t val) {
    uint64_t * p, tmp;

    if (val > 15) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Key type (%d) is too big."), val);
	return;    
    }	

    p = (uint64_t *)(&(key->u.k_offset_v2));
    tmp = LE64_TO_CPU(*p);
    tmp &= ~KEY_TYPE_MASK;
    tmp |= (val << 60);

    *p = CPU_TO_LE64(tmp);
}

uint32_t reiserfs_key_format(const struct key *key) {
    int type;

    type = get_key_v2_type(key);

    if (type == 0 || type == 15)
	return KEY_FORMAT_1;

    return KEY_FORMAT_2;
}

uint32_t reiserfs_key_type(const struct key *key) {
    if (reiserfs_key_format(key) == KEY_FORMAT_1)
	return reiserfs_key_uniq2type(get_key_v1_type(key));
	
    return get_key_v2_type(key);
}

uint64_t reiserfs_key_offset(const struct key *key) {
    if (reiserfs_key_format(key) == KEY_FORMAT_1)
	return get_key_v1_offset(key);

    return get_key_v2_offset(key);
}

void reiserfs_key_v1_form(struct key *key, uint32_t dirid, uint32_t objid, 
    uint32_t offset, uint32_t type)
{
    memset(key, 0, sizeof(*key));
    set_key_dirid(key, dirid);
    set_key_objid(key, objid);
    set_key_v1_offset(key, offset);
    set_key_v1_type(key, type);
}    

void reiserfs_key_v2_form(struct key *key, uint32_t dirid, uint32_t objid, 
    uint64_t offset, uint64_t type)
{
    memset(key, 0, sizeof(*key));
    set_key_dirid(key, dirid);
    set_key_objid(key, objid);
    set_key_v2_offset(key, offset);
    set_key_v2_type(key, type);
}    

void reiserfs_key_form(struct key *key, uint32_t dirid, uint32_t objid, 
    uint64_t offset, uint64_t type, int format)
{
    (format == FS_FORMAT_3_5 ? 
     	reiserfs_key_v1_form(key, dirid, objid, (uint32_t)offset, 
	reiserfs_key_type2uniq((uint32_t)type)) : 
	reiserfs_key_v2_form(key, dirid, objid, offset, type));
}

/* Compare functions */
int reiserfs_key_comp_dirs(void *key1, void *key2) {

    if (get_key_dirid((struct key *)key1) < get_key_dirid((struct key *)key2))
        return -1;
	
    if (get_key_dirid((struct key *)key1) > get_key_dirid((struct key *)key2))
        return 1;

    return 0;
}

int reiserfs_key_comp_objects(void *key1, void *key2) {

    if (get_key_objid((struct key *)key1) < get_key_objid((struct key *)key2))
        return -1;
	
    if (get_key_objid((struct key *)key1) > get_key_objid((struct key *)key2))
        return 1;

    return 0;
}

int reiserfs_key_comp_two_components(void *key1, void *key2) {
    uint32_t *p_key1, *p_key2;
    int key_length = SHORT_KEY_LEN;

    p_key1 = (uint32_t *)key1;
    p_key2 = (uint32_t *)key2;
	
    for(; key_length--; p_key1++, p_key2++) {
	if (LE32_TO_CPU(*p_key1) < LE32_TO_CPU(*p_key2))
	    return -1;
		
	if (LE32_TO_CPU(*p_key1) > LE32_TO_CPU(*p_key2))
	    return 1;
    }

    return 0;
}

int reiserfs_key_comp_three_components(void *key1, void *key2) {
    int retval;
	
    if ((retval = reiserfs_key_comp_two_components(key1, key2)))
	return retval;
	
    if (reiserfs_key_offset((struct key *)key1) < reiserfs_key_offset((struct key *)key2))
	return -1;

    if (reiserfs_key_offset((struct key *)key1) > reiserfs_key_offset((struct key *)key2))
	return 1;

    return 0;
}

int reiserfs_key_comp_four_components(void *key1, void *key2) {
    int retval;

    if ((retval = reiserfs_key_comp_three_components(key1, key2)))
	return retval;

    if (reiserfs_key_type((struct key *)key1) < reiserfs_key_type((struct key *)key2))
	return -1;

    if (reiserfs_key_type((struct key *)key1) > reiserfs_key_type((struct key *)key2))
	return 1;

    return 0;
}

