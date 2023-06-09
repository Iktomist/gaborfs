/*
    core.c -- reiserfs general code
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <reiserfs/reiserfs.h>
#include <reiserfs/debug.h>
#include <reiserfs/callback.h>

#define N_(String) (String)
#if ENABLE_NLS
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif

#define reiserfs_fs_bitmap_check_state(fs, action) \
    do { \
	if (!reiserfs_fs_bitmap_opened(fs)) { \
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, \
		_("Bitmap isn't opened. Possible filesystem was opened in "\
		"the \"fast\" maner.")); \
	    action; \
	} \
    } while (0)

/* Journal functions */
int reiserfs_fs_journal_opened(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
    return fs->journal ? 1 : 0;
}

blk_t reiserfs_fs_journal_size(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
    return get_jp_len(get_sb_jp(fs->super));
}

blk_t reiserfs_fs_journal_offset(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
    return get_jp_start(get_sb_jp(fs->super));
}

blk_t reiserfs_fs_journal_trans_max(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
    return get_jp_max_trans_len(get_sb_jp(fs->super));
}

blk_t reiserfs_fs_journal_area(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
    return (reiserfs_fs_journal_relocated(fs) ? 
	get_sb_reserved_for_journal(fs->super) : 
	get_jp_len(get_sb_jp(fs->super)) + 1);
}

static char *journal_kinds[] = {"standard", "relocated"};

const char *reiserfs_fs_journal_kind_str(int relocated) {
    if (relocated < 0 || relocated > 1)
	return NULL;
	
    return journal_kinds[relocated];
}

int reiserfs_fs_journal_kind(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
    return reiserfs_tools_journal_signature(fs->super->s_v1.sb_magic);
}

static void reiserfs_fs_super_magic_update(reiserfs_super_t *super, int format, 
    int relocated) 
{
    if (!relocated) {
	switch (format) {
	    case FS_FORMAT_3_5: {
		memcpy(super->s_v1.sb_magic, REISERFS_3_5_SUPER_SIGNATURE, 
		   sizeof (REISERFS_3_5_SUPER_SIGNATURE));
		break;
	    }
	    case FS_FORMAT_3_6: {
		memcpy(super->s_v1.sb_magic, REISERFS_3_6_SUPER_SIGNATURE, 
		    sizeof (REISERFS_3_6_SUPER_SIGNATURE));
		break;
	    }
	}
    } else {
	memcpy(super->s_v1.sb_magic, REISERFS_JR_SUPER_SIGNATURE, 
	    sizeof (REISERFS_JR_SUPER_SIGNATURE));
    }
}

static int reiserfs_fs_journal_tune_check(reiserfs_fs_t *fs, dal_t *dal, 
    blk_t start, blk_t len, blk_t max_trans, int relocated) 
{
    reiserfs_journal_trans_t old_trans, new_trans;
	
    ASSERT(fs != NULL, return 0);
    ASSERT(dal != NULL, return 0);
	
    if (!relocated && relocated == reiserfs_fs_journal_relocated(fs)) {
	if (start != get_jp_start(get_sb_jp(fs->super))) {
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		_("Can't change start of the standard journal."));
	    return 0;
	}
    }
	
    /* Checking for non-replayed transactions */
    memset(&old_trans, 0, sizeof(old_trans));
    memset(&new_trans, 0, sizeof(new_trans));
	
    if (reiserfs_journal_boundary_transactions(fs->journal, &old_trans, 
	&new_trans))
    {
	if (new_trans.jt_trans_id != get_jh_last_flushed(&fs->journal->head)) {
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		_("There are non-replayed transaction in old journal,"
		" check filesystem consistency first."));
	    return 0;
	}
    }
	
    return 1;
}

static int reiserfs_fs_journal_switch_to_standard(reiserfs_fs_t *fs, dal_t *dal,
    blk_t max_trans) 
{
    reiserfs_gauge_t *gauge;
    blk_t root_blk, new_len, max_len;
    reiserfs_segment_t src_segment, dst_segment;
	
    if (!get_sb_reserved_for_journal(fs->super)) {
	new_len = get_jp_len(get_sb_jp(fs->super));
		
	if (new_len >= reiserfs_fs_free_size(fs))
		new_len = reiserfs_fs_free_size(fs) - 1;
			
	/* Checking whether old relocated journal has valid size */
	max_len = reiserfs_journal_max_len(fs->dal, fs->super_off + 2, 0);

	if (new_len > max_len)
	    new_len = max_len;
		
	if (new_len < JOURNAL_MIN_SIZE)
	    new_len = JOURNAL_MIN_SIZE;

	if (new_len >= reiserfs_fs_free_size(fs)) {
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		_("Not enought free space on host device for %lu blocks "
		"of journal."),	new_len);
	    return 0;    
	}
		
	/* Relocation of the all occupied blocks */
	if (!reiserfs_segment_init(&src_segment, fs->dal, fs->super_off + 2, 
		fs->super_off + 2 + new_len))
	    return 0;	
		
	if (!reiserfs_segment_init(&dst_segment, fs->dal, fs->super_off + 2 + 
		new_len + 1, reiserfs_fs_size(fs)))
	    return 0;	
		
	if (!reiserfs_fs_state_update(fs, FS_CORRUPTED))
	    return 0;
		
	if ((gauge = libreiserfs_get_gauge())) {
	    libreiserfs_gauge_reset(gauge);
	    libreiserfs_gauge_set_name(gauge, _("relocating occupied area"));
	}
		
	if (!(root_blk = reiserfs_segment_relocate(fs, &dst_segment, 
		fs, &src_segment, 0)))
	    return 0;
			
	if (gauge)
	    libreiserfs_gauge_finish(gauge, 1);
		
	/* Updating root block */
	set_sb_root_block(fs->super, root_blk);
    } else
	new_len = get_sb_reserved_for_journal(fs->super) - 1;
	
    reiserfs_fs_journal_close(fs);
	
    /* Creating new journal on host device */
    if (!reiserfs_fs_journal_create(fs, fs->dal, fs->super_off + 2, new_len, max_trans))
	return 0;

    /* Updating free blocks */
    if (!get_sb_reserved_for_journal(fs->super))
	set_sb_free_blocks(fs->super, get_sb_free_blocks(fs->super) - (new_len + 1));
	    
    /* Updating super signature and reserved for journal field */
    reiserfs_fs_super_magic_update(fs->super, get_sb_format(fs->super), 
	!dal_equals(fs->dal, fs->journal->dal));
	
    set_sb_reserved_for_journal(fs->super, 0);
	
    if (!reiserfs_fs_state_update(fs, FS_CONSISTENT))
    	return 0;

    return 1;
}

static int reiserfs_fs_journal_switch_to_relocated(reiserfs_fs_t *fs, dal_t *dal,
    blk_t start, blk_t len, blk_t max_trans)
{
    blk_t old_len;
    
    /* Updating journal */
    reiserfs_fs_journal_close(fs);
		
    old_len = get_jp_len(get_sb_jp(fs->super));
	    
    /* Creating new journal */
    if (!reiserfs_fs_journal_create(fs, dal, start, len, max_trans))
	return 0;
		
    /* Updating reserved_for_journal field to old journal size */
    reiserfs_fs_super_magic_update(fs->super, get_sb_format(fs->super), 
	!dal_equals(fs->dal, fs->journal->dal));
		
    set_sb_reserved_for_journal(fs->super, old_len + 1);
	
    return 1;
} 

int reiserfs_fs_journal_tune(reiserfs_fs_t *fs, dal_t *dal, 
    blk_t start, blk_t len, blk_t max_trans)
{
    int relocated;
	
    ASSERT(fs != NULL, return 0);
    ASSERT(dal != NULL, return 0);
	
    reiserfs_fs_bitmap_check_state(fs, return 0);
	
    if (!reiserfs_fs_journal_opened(fs))
	return 0;
	
    relocated = !dal_equals(fs->dal, dal);
	
    if (!reiserfs_journal_params_check(dal, start, len, relocated))
	return 0;
	
    if (!reiserfs_fs_journal_tune_check(fs, dal, start, len, 
	    max_trans, relocated))
	return 0;
   
    if (!reiserfs_fs_state_update(fs, FS_CORRUPTED))
	return 0;
	
    if (relocated != reiserfs_fs_journal_relocated(fs)) {
    	if (!relocated) {
	    if (!reiserfs_fs_journal_switch_to_standard(fs, dal, 
		    max_trans))
		return 0;
	} else {
	    if (!reiserfs_fs_journal_switch_to_relocated(fs, dal, 
		    start, len, max_trans))
	        return 0;
	}
    } else {
	/*
	    If new journal is relocated and boundaries changed, then we need
	    to fill new area by zero
	*/
	if (relocated && (start != get_jp_start(get_sb_jp(fs->super)) ||
	    len != get_jp_len(get_sb_jp(fs->super))))
	{
	    reiserfs_fs_journal_close(fs);

	    /* Creating new journal */
	    if (!reiserfs_fs_journal_create(fs, dal, start, len, max_trans))
		return 0;
	}
		
	if (!relocated && start == get_jp_start(get_sb_jp(fs->super)) &&
	    len == get_jp_len(get_sb_jp(fs->super)))
	{
	    /* Journal location and boundaries are still unchnaged */
	    reiserfs_journal_params_update(&fs->journal->head.jh_params, start, len,
	        max_trans, get_jp_dev(get_sb_jp(fs->super)), 
	        get_sb_block_size(fs->super));

	    memcpy(get_sb_jp(fs->super), &fs->journal->head.jh_params, 
	        sizeof(fs->journal->head.jh_params));
	}
    }
	
    if (!reiserfs_fs_state_update(fs, FS_CONSISTENT))
        return 0;
	
    reiserfs_fs_mark_super_dirty(fs);
    reiserfs_fs_mark_bitmap_dirty(fs);
    reiserfs_fs_mark_journal_dirty(fs);

    return 1;
}

int reiserfs_fs_journal_open(reiserfs_fs_t *fs, dal_t *dal) {
    blk_t start;
    int relocated;
	
    ASSERT(fs != NULL, return 0);

    if (reiserfs_fs_journal_opened(fs)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Journal already opened."));
	return 0;    
    }

    start = get_jp_start(get_sb_jp(fs->super));
    relocated = dal && !dal_equals(fs->dal, dal);
	
    if (relocated != reiserfs_fs_journal_relocated(fs)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Journal kind mismatch has detected. Filesystem has %s journal, but "
	    "specified %s journal."), 
	    reiserfs_fs_journal_kind_str(reiserfs_fs_journal_relocated(fs)), 
	    reiserfs_fs_journal_kind_str(relocated));
	return 0;        
    }
	
    if (!(fs->journal = reiserfs_journal_open(dal, start,
	get_jp_len(get_sb_jp(fs->super)), relocated)))
    {	    
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't open journal."));
	return 0;
    }
	
    reiserfs_fs_mark_journal_clean(fs);

    return 1;
}

int reiserfs_fs_journal_reopen(reiserfs_fs_t *fs, dal_t *dal) {

    ASSERT(fs != NULL, return 0);
    ASSERT(dal != NULL, return 0);
	
    if (reiserfs_fs_journal_opened(fs))
	reiserfs_fs_journal_close(fs);
	
    return reiserfs_fs_journal_open(fs, dal);
}

int reiserfs_fs_journal_create(reiserfs_fs_t *fs, dal_t *dal, blk_t start,
    blk_t len, blk_t max_trans) 
{
    int relocated;
	
    ASSERT(fs != NULL, return 0);
	
    if (reiserfs_fs_journal_opened(fs)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Journal already opened."));
	return 0;
    }

    reiserfs_fs_bitmap_check_state(fs, return 0);
    relocated = dal && !dal_equals(fs->dal, dal);
	
    start = (relocated ? start : fs->super_off + 2);
	
    if (!(fs->journal = reiserfs_journal_create(dal ? dal : fs->dal, start, len, 
	max_trans, relocated)))
    {		
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't create journal."));
	return 0;
    }
	
    /* Updating super's journal parameters */
    memcpy(get_sb_jp(fs->super), &fs->journal->head.jh_params, 
	sizeof(fs->journal->head.jh_params));
	
    /* Marking journal blocks as used. */
    if (!relocated) {
	blk_t blk;
		
	/* Marking len and journal parameters block. */
	for (blk = start; blk < start + len + 1; blk++)
	    reiserfs_fs_bitmap_use_block(fs, blk);
    }
	
    reiserfs_fs_mark_journal_clean(fs);
    return 1;
}

int reiserfs_fs_journal_recreate(reiserfs_fs_t *fs, dal_t *dal, 
    blk_t start, blk_t len, blk_t max_trans)
{
    ASSERT(fs != NULL, return 0);
	
    reiserfs_fs_bitmap_check_state(fs, return 0);
	
    reiserfs_fs_journal_close(fs);
    return reiserfs_fs_journal_create(fs, dal, start, len, max_trans);
}    

int reiserfs_fs_journal_sync(reiserfs_fs_t *fs) {

    ASSERT(fs != NULL, return 0);
	
    reiserfs_fs_bitmap_check_state(fs, return 0);

    if (!reiserfs_fs_journal_opened(fs)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Journal isn't opened."));
	return 0;
    }

    if (!reiserfs_journal_sync(fs->journal))
	return 0;
	
    reiserfs_fs_mark_journal_clean(fs);
	
    return 1;	
}

void reiserfs_fs_journal_close(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return);
	
    if (!reiserfs_fs_journal_opened(fs)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Journal isn't opened."));
	return;
    }
	
    reiserfs_journal_close(fs->journal);
    fs->journal = NULL;
}

reiserfs_journal_t *reiserfs_fs_journal(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return NULL);
    return fs->journal;
}

/* Bitmap functions */
int reiserfs_fs_bitmap_opened(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
    return fs->bitmap ? 1 : 0;
}

void reiserfs_fs_bitmap_use_block(reiserfs_fs_t *fs, blk_t block) {
    ASSERT(fs != NULL, return);

    reiserfs_fs_bitmap_check_state(fs, return);
	
    reiserfs_bitmap_use_block(fs->bitmap, block);
    reiserfs_fs_mark_bitmap_dirty(fs);
}

void reiserfs_fs_bitmap_unuse_block(reiserfs_fs_t *fs, blk_t block) {

    ASSERT(fs != NULL, return);
	
    reiserfs_fs_bitmap_check_state(fs, return);
	
    reiserfs_bitmap_unuse_block(fs->bitmap, block);
    reiserfs_fs_mark_bitmap_dirty(fs);
}

int reiserfs_fs_bitmap_test_block(reiserfs_fs_t *fs, blk_t block) {
	
    ASSERT(fs != NULL, return 0);
	
    reiserfs_fs_bitmap_check_state(fs, return 0);
    return reiserfs_bitmap_test_block(fs->bitmap, block);
}

blk_t reiserfs_fs_bitmap_find_free_block(reiserfs_fs_t *fs, 
    blk_t start)
{
    ASSERT(fs != NULL, return 0);
	
    reiserfs_fs_bitmap_check_state(fs, return 0);
    return reiserfs_bitmap_find_free(fs->bitmap, start);
}

blk_t reiserfs_fs_bitmap_calc_used(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
	
    reiserfs_fs_bitmap_check_state(fs, return 0);
    return reiserfs_bitmap_calc_used(fs->bitmap);
}

blk_t reiserfs_fs_bitmap_calc_unused(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
	
    reiserfs_fs_bitmap_check_state(fs, return 0);
    return reiserfs_bitmap_calc_unused(fs->bitmap);
}

blk_t reiserfs_fs_bitmap_used(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
	
    reiserfs_fs_bitmap_check_state(fs, return 0);
    return reiserfs_bitmap_used(fs->bitmap);
}

blk_t reiserfs_fs_bitmap_unused(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
	
    reiserfs_fs_bitmap_check_state(fs, return 0);
    return reiserfs_bitmap_unused(fs->bitmap);
}

int reiserfs_fs_bitmap_check(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
	
    reiserfs_fs_bitmap_check_state(fs, return 0);
    return reiserfs_bitmap_check(fs->bitmap);
}

int reiserfs_fs_bitmap_open(reiserfs_fs_t *fs) {
	
    ASSERT(fs != NULL, return 0);
	
    if (reiserfs_fs_bitmap_opened(fs)) {
    	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL,
	    _("Bitmap already opened."));
	return 0;
    }

    if (!(fs->bitmap = reiserfs_bitmap_open(fs, fs->super_off + 1,
					    get_sb_block_count(fs->super)))) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't open bitmap."));
	return 0;
    }

    reiserfs_fs_mark_bitmap_clean(fs);
    return 1;
}

int reiserfs_fs_bitmap_create(reiserfs_fs_t *fs, size_t blocksize, blk_t fs_len) {

    ASSERT(fs != NULL, return 0);
	
    if (reiserfs_fs_bitmap_opened(fs)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL,
	    _("Bitmap already opened."));
	return 0;
    }
	
    if (!(fs->bitmap = reiserfs_bitmap_create(fs, (DEFAULT_SUPER_OFFSET / blocksize) + 1, 
	fs_len)))
    {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't create bitmap."));
	return 0;
    }
	
    reiserfs_fs_mark_bitmap_clean(fs);
    return 1;
}

static void reiserfs_fs_bitmap_mark(reiserfs_fs_t *fs, reiserfs_segment_t *segment, int mark) {
    blk_t i;
	
    for (i = 0; i < reiserfs_segment_len(segment); i++) {
	mark ? reiserfs_fs_bitmap_use_block(fs, segment->start + i) : 
	    reiserfs_fs_bitmap_unuse_block(fs, segment->start + i);
    }	
}

int reiserfs_fs_bitmap_resize(reiserfs_fs_t *fs, long start, long end) {
	
    ASSERT(fs != NULL, return 0);
	
    reiserfs_fs_bitmap_check_state(fs, return 0);

    if (!reiserfs_bitmap_resize(fs->bitmap, start, end)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't resize bitmap to (%lu - %lu) blocks."), start, end);
	return 0;
    }
    
    reiserfs_fs_mark_bitmap_dirty(fs);
	
    return 1;
}

int reiserfs_fs_bitmap_sync(reiserfs_fs_t *fs) {

    ASSERT(fs != NULL, return 0);
	
    reiserfs_fs_bitmap_check_state(fs, return 0);
	
    if (!reiserfs_bitmap_sync(fs->bitmap))
	return 0;

    reiserfs_fs_mark_bitmap_clean(fs);
	
    return 1;	
}    
	
void reiserfs_fs_bitmap_close(reiserfs_fs_t *fs) {
	
    ASSERT(fs != NULL, return);
	
    reiserfs_fs_bitmap_check_state(fs, return);
    reiserfs_bitmap_close(fs->bitmap);
    fs->bitmap = NULL;
}

int reiserfs_fs_bitmap_reopen(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
    reiserfs_fs_bitmap_close(fs);
    return reiserfs_fs_bitmap_open(fs);
}    

reiserfs_bitmap_t *reiserfs_fs_bitmap(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return NULL);
    return fs->bitmap;
}

/* Superblock functions */
int reiserfs_fs_super_sync(reiserfs_fs_t *fs) {
    reiserfs_block_t *block;
	
    ASSERT(fs != NULL, return 0);
    ASSERT(fs->dal != NULL, return 0);
   
    if (!(block = reiserfs_block_alloc_with_copy(fs->dal, fs->super_off, 
				    fs->super)))
	   goto error;

    if (!reiserfs_block_write(fs->dal, block)) {
	reiserfs_block_writing_failed(fs->super_off, 
	    dal_error(fs->dal), goto error_free_block);
    }
    
    reiserfs_block_free(block);
    
    reiserfs_fs_mark_super_clean(fs);
    return 1;
	
error_free_block:
    reiserfs_block_free(block);
error:
    return 0;
}

static int reiserfs_fs_super_open_check(reiserfs_super_t *sb, blk_t dev_len, int quiet) {
    int is_journal_dev, is_journal_magic;
	
    ASSERT(sb != NULL, return 0);
    ASSERT(dev_len > 0, return 0);
	
    is_journal_dev = (get_jp_dev(get_sb_jp(sb)) ? 1 : 0);
    is_journal_magic = reiserfs_tools_journal_signature(sb->s_v1.sb_magic);
	    
    if (is_journal_dev != is_journal_magic && !quiet) {
	libreiserfs_exception_throw(EXCEPTION_WARNING, EXCEPTION_IGNORE, 
	    _("Journal relocation flags mismatch. Journal device: %x, magic: %s."),
	    get_jp_dev(get_sb_jp(sb)), sb->s_v1.sb_magic);
    }
	
    if (get_sb_block_count(sb) > dev_len) {
	if (!quiet) {
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		_("Superblock has an invalid block count %lu for device "
		"length %lu blocks."), get_sb_block_count(sb), dev_len);
	}
	return 0;    
    }
	
    return 1;
}

static reiserfs_block_t *reiserfs_fs_super_probe(dal_t *dal, int quiet) {
    reiserfs_super_t *super;
    reiserfs_block_t *block;
    int i, super_offset[] = {16, 2, -1};

    ASSERT(dal != NULL, return NULL);

    for (i = 0; super_offset[i] != -1; i++) {
	if (!(block = reiserfs_block_read(dal, super_offset[i])) && !quiet ) {
	    libreiserfs_exception_throw(EXCEPTION_WARNING, EXCEPTION_IGNORE, 
		_("Reading block %lu for blocksize %d failed. %s."), super_offset[i], 
		dal_get_blocksize(dal), dal_error(dal));
	} else {
	    super = (reiserfs_super_t *)block->data;
		
	    if (reiserfs_tools_any_signature((const char *)super->s_v1.sb_magic)) {
		/* 
		    Making some checks to make sure super block looks
		    correctly.
		*/
		if (!dal_set_blocksize(dal, get_sb_block_size(super)) && !quiet) {
		    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
			_("Invalid blocksize %d. It must power of two."), 
			get_sb_block_size(super));
			reiserfs_block_free(block);
		    continue;
		}
			
		if (!reiserfs_fs_super_open_check(super, dal_len(dal), quiet)) {
		    reiserfs_block_free(block);
		    continue;	
		}
		return block;
	    }
	    reiserfs_block_free(block);
	}
    }
	
    return NULL;
}

int reiserfs_fs_super_reopen(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);

    reiserfs_fs_super_close(fs);
    return reiserfs_fs_super_open(fs);
}

int reiserfs_fs_super_open(reiserfs_fs_t *fs) {
    reiserfs_block_t *block;

    ASSERT(fs != NULL, return 0);
	
    if (fs->super) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Superblock already opened."));
	return 0;
    }
	
    if (reiserfs_fs_journal_opened(fs) && fs->super_off) {
	if (!(block = reiserfs_journal_read(fs->journal, fs->super_off))) {
	    if (!(block = reiserfs_block_read(fs->dal, fs->super_off))) {
		libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		    _("Couldn't reopen superblock from journal. %s."), 
		    dal_error(fs->dal));
		return 0;
	    }
	}
	
	if (!reiserfs_fs_super_open_check((reiserfs_super_t *)block->data, 
	    dal_len(fs->dal), 0)) 
	{
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		_("Invalid superblock has read from the journal."));
	    goto error_free_block;
	}

	reiserfs_block_set_nr(block, fs->super_off);
    } else {
	if (!(block = reiserfs_fs_super_probe(fs->dal, 0)))
	    return 0;
    }
	
    if (!(fs->super = libreiserfs_calloc(dal_get_blocksize(fs->dal), 0)))
	goto error_free_block;
	
    memcpy(fs->super, block->data, dal_get_blocksize(fs->dal));
    fs->super_off = reiserfs_block_get_nr(block);
	
    reiserfs_fs_mark_super_clean(fs);
    reiserfs_block_free(block);
	
    return 1;
	
error_free_block:
    reiserfs_block_free(block);	
error:
    return 0;	
}

int reiserfs_fs_clobber_skipped(dal_t *dal) {
    reiserfs_gauge_t *gauge;
    reiserfs_segment_t segment;
    reiserfs_block_t *block;
    size_t orig_blocksize;

    ASSERT(dal != NULL, return 0);
	
    orig_blocksize = dal_get_blocksize(dal);
	
    if (!dal_set_blocksize(dal, 1024))
	goto error;
	
#if defined(__sparc__) || defined(__sparcv9)
    if (!reiserfs_segment_init(&segment, dal, 1, DEFAULT_SUPER_OFFSET / dal_get_blocksize(dal)))
	goto error;
#else    
    if (!reiserfs_segment_init(&segment, dal, 0, DEFAULT_SUPER_OFFSET / dal_get_blocksize(dal)))
	goto error;
#endif

    if ((gauge = libreiserfs_get_gauge())) {
	libreiserfs_gauge_reset(gauge);
	libreiserfs_gauge_set_name(gauge, _("initializing skiped area"));
    }
	
    if (!reiserfs_segment_fill(&segment, 0, 
	    (reiserfs_segment_func_t)reiserfs_callback_segment_gauge, gauge))
	goto error;

    if (gauge)
	libreiserfs_gauge_finish(gauge, 1);
	
    return dal_set_blocksize(dal, orig_blocksize);
	
error:
    dal_set_blocksize(dal, orig_blocksize);
    return 0;
}

int reiserfs_fs_super_create(reiserfs_fs_t *fs, int format, reiserfs_hash_t hash,
    const char *label, const char *uuid, size_t blocksize, blk_t start, 
    blk_t len, blk_t fs_len, int relocated)
{
    blk_t sb_blk, blk;
    reiserfs_super_t *sb;
    reiserfs_block_t *block;

    ASSERT(fs != NULL, return 0);
	
    reiserfs_fs_bitmap_check_state(fs, return 0);
	
    if (!reiserfs_fs_clobber_skipped(fs->dal))
	return 0;

    sb_blk = DEFAULT_SUPER_OFFSET / blocksize;

    if (!(block = reiserfs_block_alloc(fs->dal, sb_blk, 0)))
	return 0;
	
    sb = (reiserfs_super_t *)block->data;
	
    set_sb_umount_state(sb, FS_CLEAN);

    sb->s_v1.sb_block_count = CPU_TO_LE32(fs_len);
    sb->s_v1.sb_bmap_nr = CPU_TO_LE16((fs_len - 1) / (8 * blocksize) + 1);

    /* 
	Used blocks are: blk-s skipped, super block, 
	bitmap blocks, journal, root block.
    */
	
    sb->s_v1.sb_free_blocks = CPU_TO_LE32(fs_len - sb_blk - 1 - 
	(relocated ? 0 : len + 1) - LE16_TO_CPU(sb->s_v1.sb_bmap_nr) - 1);
	
    sb->s_v1.sb_format = CPU_TO_LE16(format);
    sb->s_v1.sb_block_size = CPU_TO_LE16(blocksize);
    sb->s_v1.sb_fs_state = CPU_TO_LE16(FS_CONSISTENT);

    /* Hash function */
    sb->s_v1.sb_hash_function_code = CPU_TO_LE32(hash);
	
    /* Updating super signature */
    reiserfs_fs_super_magic_update(sb, format, relocated);

    if (format == FS_FORMAT_3_6)
	sb->s_v1.sb_oid_maxsize = CPU_TO_LE16 ((blocksize - SUPER_V2_SIZE) /
	    sizeof (uint32_t) / 2 * 2);
    else	    
	sb->s_v1.sb_oid_maxsize = CPU_TO_LE16 ((blocksize - SUPER_V1_SIZE) / 
	    sizeof (uint32_t) / 2 * 2);
	    
    /* Label and uuid */
    if (label && strlen(label)) {
    	int label_len = strlen(label) < sizeof(sb->s_label) ? strlen(label) : 
	    sizeof(sb->s_label) - 1;
	memcpy(sb->s_label, label, label_len);
    }	

    if (uuid && strlen(uuid)) {
	int uuid_len = strlen(uuid) < sizeof(sb->s_uuid) ? strlen(uuid) : 
	    sizeof(sb->s_uuid) - 1;
	memcpy(sb->s_uuid, uuid, uuid_len);
    }	

    /* Journal params */
    reiserfs_journal_params_update(get_sb_jp(sb), start, len, 0, 
	relocated, get_sb_block_size(sb));
	
    if (!(fs->super = (reiserfs_super_t *)libreiserfs_calloc(blocksize, 0)))
	goto error_free_block;
	
    memcpy(fs->super, sb, blocksize);

    fs->super_off = sb_blk;
    reiserfs_block_free(block);
	
    /* Marking skiped blocks used and super block as used. */
    for (blk = 0; blk <= sb_blk; blk++)
    	reiserfs_fs_bitmap_use_block(fs, blk);

    reiserfs_fs_mark_super_dirty(fs);
    reiserfs_fs_mark_bitmap_dirty(fs);
	
    return 1;
	
error_free_block:
    reiserfs_block_free(block);
error:
    return 0;    
}

dal_t *reiserfs_fs_dal(reiserfs_fs_t *fs) {
    return fs->dal;
}

int reiserfs_fs_set_root(reiserfs_fs_t *fs, blk_t blk) {
    ASSERT(fs != NULL, return 0);
	
    set_sb_root_block(fs->super, blk);
    reiserfs_fs_mark_super_dirty(fs);
	
    return 1;
}

void *reiserfs_fs_data(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return NULL);
    return fs->data;
}

void reiserfs_fs_set_data(reiserfs_fs_t *fs, void *data) {
    ASSERT(fs != NULL, return);
    fs->data = data;
}

void reiserfs_fs_super_close(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return);
    libreiserfs_free(fs->super);
    fs->super = NULL;
}

reiserfs_super_t *reiserfs_fs_super(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return NULL);
    return fs->super;
}

blk_t reiserfs_fs_probe(dal_t *dal) {
    blk_t fs_len;
    reiserfs_block_t *block;
    reiserfs_super_t *super;

    ASSERT(dal != NULL, return 0);
	
    dal_set_blocksize(dal, DEFAULT_BLOCK_SIZE);
	
    if (!(block = reiserfs_fs_super_probe(dal, 1)))
	return 0;
	
    super = (reiserfs_super_t *)block->data;

    fs_len = get_sb_block_count(super);
    reiserfs_block_free(block);
	
    return fs_len;
}

static int reiserfs_fs_resize_check(reiserfs_fs_t *fs) {
    if (!reiserfs_fs_is_resizeable(fs)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL,
	    _("Can't resize old format filesystem."));
	return 0;
    }
   
    reiserfs_fs_bitmap_check_state(fs, return 0);
	
    if (get_sb_umount_state(fs->super) != FS_CLEAN) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL,
	    _("Filesystem isn't in valid state. May be it is not cleanly unmounted."));
	return 0;
    }
	
    return 1;
}

/* Smart resizing stuff */
static int reiserfs_fs_metadata_move(reiserfs_fs_t *fs, long start, long end) {
    reiserfs_gauge_t *gauge;
    reiserfs_segment_t src_segment, dst_segment;
	
    blk_t old_meta_off = fs->super_off + (start < 0 ? labs(start) : 0);
    blk_t new_meta_off = fs->super_off + (start < 0 ? 0 : labs(start));
    blk_t meta_len = 2 + reiserfs_fs_journal_area(fs);
	
    /* Moving the metadata */
    if (!reiserfs_segment_init(&src_segment, fs->dal, old_meta_off, old_meta_off + 
	    meta_len))
	return 0;

    if (!reiserfs_segment_init(&dst_segment, fs->dal, new_meta_off, new_meta_off + 
	    meta_len))
	return 0;
	
    if ((gauge = libreiserfs_get_gauge())) {
	libreiserfs_gauge_reset(gauge);
	libreiserfs_gauge_set_name(gauge, _("relocating metadata"));
    }
	
    if (!reiserfs_segment_move(&dst_segment, &src_segment, 
	    (reiserfs_segment_func_t)reiserfs_callback_segment_gauge, gauge))
	return 0;
    
    if (gauge)
	libreiserfs_gauge_finish(gauge, 1);

    return 1;
}

static blk_t reiserfs_fs_tree_move(reiserfs_fs_t *fs, long start, long end) {
    reiserfs_gauge_t *gauge;
    reiserfs_segment_t src_segment, dst_segment;

    blk_t root_blk;
	
    blk_t old_meta_off = fs->super_off + (start < 0 ? labs(start) : 0);
    blk_t new_meta_off = fs->super_off + (start < 0 ? 0 : labs(start));
    blk_t meta_len = 2 + reiserfs_fs_journal_area(fs);
	
    /* Moving the tree */
    reiserfs_tree_set_offset(fs->tree, (start < 0 ? start : 0));
	
    if (!reiserfs_segment_init(&src_segment, fs->dal, old_meta_off + meta_len, 
	    reiserfs_fs_size(fs) + -start))
	return 0;

    if (!reiserfs_segment_init(&dst_segment, fs->dal, new_meta_off + meta_len, end))
	return 0;

    if ((gauge = libreiserfs_get_gauge())) {
	libreiserfs_gauge_reset(gauge);
	libreiserfs_gauge_set_name(gauge, _("shrinking"));
    }
	
    if (!(root_blk = reiserfs_segment_relocate(fs, &dst_segment, fs, &src_segment, 1)))
	return 0;
	
    if (gauge)
	libreiserfs_gauge_finish(gauge, 1);

    reiserfs_tree_set_offset(fs->tree, 0);

    return root_blk;
}

int reiserfs_fs_resize_smart(reiserfs_fs_t *fs, long start, long end) {
    blk_t root_blk, fs_len;
    blk_t bitmap_new_size;

    if (!reiserfs_fs_resize_check(fs))
	return 0;
	
    if (start == 0 && end == (long)get_sb_block_count(fs->super)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL,
	    _("New boundaries are the same as previous ones."));
	return 0;
    }

    if (end < start) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL,
	    _("Invalid boundaries: start=%ld, end=%ld"), start, end);
	return 0;
    }

    fs_len = (blk_t)(end - start);
    bitmap_new_size = (fs_len - 1) / (8 * get_sb_block_size(fs->super)) + 1;

    /* Check if we are going to shrink */	
    if (get_sb_block_count(fs->super) > fs_len) {
	if (get_sb_block_count(fs->super) - fs_len > 
			get_sb_free_blocks(fs->super) + 
	    get_sb_bmap_nr(fs->super) - bitmap_new_size)
	{
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL,
		_("Can't shrink filesystem. Too many "
			"blocks already allocated."));
	    return 0;
	}
    }
    
    if (!reiserfs_fs_state_update(fs, FS_CORRUPTED))
	return 0;
	
    if (!reiserfs_fs_bitmap_resize(fs, start, end))
	return 0;

    if (start < 0) {
	if (!reiserfs_fs_metadata_move(fs, start, end))
	    return 0;
		
	if (!(root_blk = reiserfs_fs_tree_move(fs, start, end)))
	    return 0;
    } else {
	if (!(root_blk = reiserfs_fs_tree_move(fs, start, end)))
	    return 0;
		
	if (!reiserfs_fs_metadata_move(fs, start, end))
	    return 0;
    }
	
    fs->super_off = (start <= 0 ? fs->super_off : fs->super_off + start);

    set_sb_root_block(fs->super, root_blk);
	
    set_sb_free_blocks(fs->super, get_sb_free_blocks(fs->super) - 
	(get_sb_block_count(fs->super) - fs_len) + (get_sb_bmap_nr(fs->super) - 
	bitmap_new_size));

    set_sb_block_count(fs->super, fs_len);
    set_sb_bmap_nr(fs->super, bitmap_new_size);

    reiserfs_fs_mark_bitmap_dirty(fs);
    reiserfs_fs_mark_super_dirty(fs);

    if (!reiserfs_fs_state_update(fs, FS_CONSISTENT))
	return 0;

    if (!reiserfs_fs_super_sync(fs))
	return 0;
    
    fs->super_off = DEFAULT_SUPER_OFFSET / fs->dal->blocksize;
    
    return 1;
}

/* Dumb resizing stuff */
static int reiserfs_fs_shrink(reiserfs_fs_t *fs, blk_t fs_len) {
    blk_t root_blk;
    blk_t bitmap_new_size;
    reiserfs_gauge_t *gauge;
    reiserfs_segment_t src_segment, dst_segment;
	
    bitmap_new_size = (fs_len - 1) / (8 * get_sb_block_size(fs->super)) + 1;

    if (get_sb_block_count(fs->super) - fs_len > get_sb_free_blocks(fs->super) + 
	get_sb_bmap_nr(fs->super) - bitmap_new_size) 
    {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL,
	    _("Can't shrink filesystem. Too many blocks already allocated."));
	return 0;
    }
   
    if (!reiserfs_segment_init(&src_segment, fs->dal, fs_len, reiserfs_fs_size(fs)))
	return 0;

    if (!reiserfs_segment_init(&dst_segment, fs->dal, fs->super_off + 2 + 
	    reiserfs_fs_journal_area(fs), fs_len))
	return 0;
   
    if (!reiserfs_fs_state_update(fs, FS_CORRUPTED))
	return 0;
	
    if ((gauge = libreiserfs_get_gauge())) {
	libreiserfs_gauge_reset(gauge);
	libreiserfs_gauge_set_name(gauge, _("shrinking"));
    }
	
    if (!(root_blk = reiserfs_segment_relocate(fs, &dst_segment, fs, &src_segment, 0)))
	return 0;
	
    if (gauge)
	libreiserfs_gauge_finish(gauge, 1);

    if (!reiserfs_fs_bitmap_resize(fs, 0, fs_len))
	return 0;
		
    set_sb_free_blocks(fs->super, get_sb_free_blocks(fs->super) - 
	(get_sb_block_count(fs->super) - fs_len) + (get_sb_bmap_nr(fs->super) - 
	bitmap_new_size));

    set_sb_block_count(fs->super, fs_len);
    set_sb_bmap_nr(fs->super, bitmap_new_size);
		
    reiserfs_fs_mark_bitmap_dirty(fs);

    return reiserfs_fs_state_update(fs, FS_CONSISTENT);
}

static int reiserfs_fs_expand(reiserfs_fs_t *fs, blk_t fs_len) {
    blk_t bmap_new_blknr, bmap_old_blknr;

    if (fs_len > dal_len(fs->dal)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL,
	    _("Device is too small for (%lu) blocks."), fs_len);
	return 0;
    }

    bmap_old_blknr = get_sb_bmap_nr(fs->super);

    /* 
	Computing bitmap blocks count in new fs. One bit in bitmap
	points to one block.
    */
    bmap_new_blknr = (fs_len - 1) / (get_sb_block_size(fs->super) * 8) + 1;

    if (!reiserfs_fs_state_update(fs, FS_CORRUPTED))
	return 0;
	
    if (!reiserfs_fs_bitmap_resize(fs, 0, fs_len))
	return 0;

    set_sb_free_blocks(fs->super, get_sb_free_blocks(fs->super) + 
	(fs_len - get_sb_block_count(fs->super)) - (bmap_new_blknr - bmap_old_blknr));
	
    set_sb_block_count(fs->super, fs_len);
    set_sb_bmap_nr(fs->super, bmap_new_blknr);

    reiserfs_fs_mark_bitmap_dirty(fs);
    return reiserfs_fs_state_update(fs, FS_CONSISTENT);
}

int reiserfs_fs_resize_dumb(reiserfs_fs_t *fs, blk_t fs_len) {

    ASSERT(fs != NULL, return 0);
   
    if (!reiserfs_fs_resize_check(fs))
	return 0;
	
    if (fs_len == get_sb_block_count(fs->super)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL,
	    _("New size is the same as previous one."));
	return 0;
    }

    return (fs_len > get_sb_block_count(fs->super) ? 
	reiserfs_fs_expand(fs, fs_len) : reiserfs_fs_shrink(fs, fs_len));
}

int reiserfs_fs_tree_open(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
	
    if (!(fs->tree = reiserfs_tree_open((void *)fs))) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't open reiserfs tree."));
	return 0;
    }
	
    return 1;	
}

int reiserfs_fs_tree_create(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
	
    reiserfs_fs_bitmap_check_state(fs, return 0);
    if (!(fs->tree = reiserfs_tree_create((void *)fs))) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't open reiserfs tree."));
	return 0;
    }
    return 1;	
}

void reiserfs_fs_tree_close(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return);
	
    if (!fs->tree) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Tree isn't opened."));
	return;    
    }
    reiserfs_tree_free(fs->tree);
}

void *reiserfs_fs_tree(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return NULL);
    return fs->tree;
}

static reiserfs_fs_t *reiserfs_fs_open_as(dal_t *host_dal, dal_t *journal_dal, 
    int with_bitmap) 
{
    reiserfs_fs_t *fs;
    reiserfs_super_t *sb;

    ASSERT(host_dal != NULL, return NULL);

    if (!(fs = (reiserfs_fs_t *)libreiserfs_calloc(sizeof(*fs), 0)))
	goto error;
	
    fs->dal = host_dal;
	
    if (!reiserfs_fs_super_open(fs))
	goto error_free_fs;
	
    if (journal_dal)
	dal_set_blocksize(journal_dal, get_sb_block_size(fs->super));
	
    if (with_bitmap && !reiserfs_fs_is_consistent(fs)) {
	if (dal_flags(host_dal) & O_RDWR) {
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		_("Filesystem isn't consistent. Couldn't open it for write."));
	    goto error_free_fs;
	}
	if (dal_flags(host_dal) & O_RDONLY)
	    libreiserfs_exception_throw(EXCEPTION_WARNING, EXCEPTION_IGNORE, 
		_("Filesystem isn't consistent."));
    }
	
    if (get_jp_magic(get_sb_jp(fs->super)) != JOURNAL_NEED_TUNE) {
	if (reiserfs_fs_journal_relocated(fs) && journal_dal && 
	    dal_equals(host_dal, journal_dal)) 
	{
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		_("Filesystem has journal on separate device, but specified host device."));
	    goto error_free_fs;
	}
		
	if (journal_dal) {
	    if (!reiserfs_fs_journal_open(fs, journal_dal))
		goto error_free_super;
		
	    /* Reopening the superblock that can logged by the journal */
	    if (!reiserfs_fs_super_reopen(fs))
	        goto error_free_fs;
	}
    } else
	libreiserfs_exception_throw(EXCEPTION_WARNING, EXCEPTION_IGNORE, 
	    _("Journal was not opened. Journal tuning is needed."));
	
    if (with_bitmap && !reiserfs_fs_bitmap_open(fs))
	goto error_free_journal;

    if (!reiserfs_fs_tree_open(fs))
	goto error_free_journal;
	    
    return fs;
	
error_free_journal:
    if (reiserfs_fs_journal_opened(fs))
	reiserfs_fs_journal_close(fs);
error_free_super:
    reiserfs_fs_super_close(fs);    
error_free_fs:
    libreiserfs_free(fs);
error:
    return NULL;    
}

reiserfs_fs_t *reiserfs_fs_open(dal_t *host_dal, dal_t *journal_dal) {
    return reiserfs_fs_open_as(host_dal, journal_dal, 1); 
}

reiserfs_fs_t *reiserfs_fs_open_fast(dal_t *host_dal, dal_t *journal_dal) {
    return reiserfs_fs_open_as(host_dal, journal_dal, 0); 
}

int reiserfs_fs_sync(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
	
    if (reiserfs_fs_super_dirty(fs) && !reiserfs_fs_super_sync(fs))
	return 0;
	
    if (reiserfs_fs_bitmap_opened(fs) && reiserfs_fs_bitmap_dirty(fs) && 
	    !reiserfs_fs_bitmap_sync(fs))
	return 0;

    if (reiserfs_fs_journal_opened(fs) && reiserfs_fs_journal_dirty(fs) &&
	    !reiserfs_fs_journal_sync(fs))
	return 0;
	
    return 1;
}

int reiserfs_fs_clobber(dal_t *dal) {
    reiserfs_block_t *block;
    int i, super_offset[] = {16, 2, -1};

    ASSERT(dal != NULL, return 0);

    for (i = 0; super_offset[i] != -1; i++) {
	if (!(block = reiserfs_block_alloc(dal, super_offset[i], 0)))
	    return 0;

	if (!reiserfs_block_write(dal, block)) {
	    reiserfs_block_writing_failed(super_offset[i], 
		dal_error(dal), goto error_free_block);
	}
	
	reiserfs_block_free(block);    
    }
    
    return 1;
error_free_block:
    reiserfs_block_free(block);
error:
    return 0;
}

static int reiserfs_fs_create_check(dal_t *host_dal, dal_t *journal_dal, 
    blk_t start, blk_t max_trans, blk_t len, size_t blocksize, 
    int format, reiserfs_hash_t hash, const char *label, const char *uuid, 
    blk_t fs_len, int relocated)
{
    blk_t dev_len, tree_start;
	
    ASSERT(host_dal != NULL, return 0);
	
    if (!reiserfs_tools_power_of_two(blocksize)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Block size isn't power of two (%d)."), blocksize);
	return 0;
    }
	
    if (blocksize < 1024) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Block size is too small (%d)."), blocksize);
	return 0;
    }
	
    if (blocksize > DEFAULT_SUPER_OFFSET) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Block size is too big (%d)."), blocksize);
	return 0;
    }
	
    if (fs_len <= 0) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Invalid filesystem size (%lu)."), fs_len);
	return 0;
    }

    dev_len = dal_len(host_dal);
    if (fs_len > dev_len) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Filesystem is too big for device (%lu)."), dev_len);
	return 0;
    }
	
    tree_start = (DEFAULT_SUPER_OFFSET / dal_get_blocksize(host_dal)) + 2 + 
	(relocated ? 0 : len + 1);
	
    if (fs_len <= tree_start + 100) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Size of file system is too small. It must be at least (%lu) blocks."), 
	    tree_start + 100 + 1);
	return 0;
    }	
	
    return 1;
} 

reiserfs_fs_t *reiserfs_fs_create(dal_t *host_dal, dal_t *journal_dal, 
    blk_t start, blk_t max_trans, blk_t len, size_t blocksize, int format, 
    reiserfs_hash_t hash, const char *label, const char *uuid, blk_t fs_len)
{
    reiserfs_fs_t *fs;
    reiserfs_block_t *root;
    int relocated;
	
    ASSERT(host_dal != NULL, return NULL);
	
    relocated = journal_dal && !dal_equals(host_dal, journal_dal);
	
    if (!reiserfs_fs_create_check(host_dal, journal_dal, start, max_trans, 
	    len, blocksize, format, hash, label, uuid, fs_len, relocated))
	return 0;
	
    if (!(fs = (reiserfs_fs_t *)libreiserfs_calloc(sizeof(*fs), 0)))
	return NULL;
	
    fs->dal = host_dal;

    /* 
	Creating bitmap. Must be first, because all another code of 
	reiserfs_fs_create uses it.
    */
    if (!reiserfs_fs_bitmap_create(fs, blocksize, fs_len))
	goto error_free_fs;
	
    /* Creating super */
    if (!reiserfs_fs_super_create(fs, format, hash, label, uuid, blocksize, 
	start, len, fs_len, relocated)) 
    {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't create superblock."));
	goto error_free_bitmap;
    }
	
    /* Creating journal */
    if (journal_dal) {
	if (!reiserfs_fs_journal_create(fs, journal_dal, start, len, max_trans))
	    goto error_free_super;
    }

    /* Creating empty tree */
    if (!reiserfs_fs_tree_create(fs))
	goto error_free_journal;
	
    /* Synchronizing created filesystem */
    if (!reiserfs_fs_sync(fs))
	goto error_free_tree;
	
    return fs;
	
error_free_tree:
    reiserfs_fs_tree_close(fs);
error_free_journal:
    reiserfs_fs_journal_close(fs);
error_free_super:
    reiserfs_fs_super_close(fs);
error_free_bitmap:
    reiserfs_fs_bitmap_close(fs);
error_free_fs:
    libreiserfs_free(fs);    
error:
    return NULL;    
}

void reiserfs_fs_close(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return);
	
    if (!reiserfs_fs_sync(fs))
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't synchronize filesystem."));
	
    if (reiserfs_fs_journal_opened(fs))
	reiserfs_fs_journal_close(fs);
	
    if (reiserfs_fs_bitmap_opened(fs))
	reiserfs_fs_bitmap_close(fs);
	
    reiserfs_fs_tree_close(fs);
    reiserfs_fs_super_close(fs);
	
    libreiserfs_free(fs);
}

int reiserfs_fs_is_consistent(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);

    return (get_sb_fs_state(fs->super) == FS_CONSISTENT &&
	get_sb_umount_state(fs->super) == FS_CLEAN);
}

int reiserfs_fs_is_resizeable(reiserfs_fs_t *fs) {
    return fs->super_off != 2;
}

reiserfs_hash_func_t reiserfs_fs_hash_func(reiserfs_hash_t hash_code) {
    switch (hash_code) {
	case TEA_HASH: return __tea_hash_func;
	case YURA_HASH: return __yura_hash_func;
	case R5_HASH: return __r5_hash_func;
	default: return NULL;
    }
}

uint32_t reiserfs_fs_hash_value(reiserfs_fs_t *fs, const char *name) {
    uint32_t hash_value;
    reiserfs_hash_func_t hash_func;
	
    ASSERT(fs != NULL, return 0);
    ASSERT(name != NULL, return 0);
	
    if (!strcmp(name, "."))
	return DOT_OFFSET;
	
    if (!strcmp(name, ".."))
	return DOT_DOT_OFFSET;
	
    hash_func = reiserfs_fs_hash_func(reiserfs_fs_hash(fs));
    hash_value = hash_func(name, strlen(name));
	
    hash_value = GET_HASH_VALUE(hash_value);
	
    if (hash_value == 0)
	hash_value = 128;

    return hash_value;	
}

reiserfs_fs_t *reiserfs_fs_copy(reiserfs_fs_t *src_fs, dal_t *dst_dal) {
    blk_t root_blk;
    reiserfs_fs_t *dst_fs;
    reiserfs_gauge_t *gauge;
    char label[16], uuid[16];
    blk_t needed_blocks, dst_blocks;
	
    reiserfs_segment_t src_segment, dst_segment;

    ASSERT(src_fs != NULL, return NULL);
    ASSERT(dst_dal != NULL, return NULL);
	
    reiserfs_fs_bitmap_check_state(src_fs, return 0);
	
    if (dal_get_blocksize(src_fs->dal) != dal_get_blocksize(dst_dal)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Block sizes for source and destination devices are different. "
	    "Source: %d, destination: %d."), dal_get_blocksize(src_fs->dal), 
	    dal_get_blocksize(dst_dal));
	goto error;
    }
	
    if (!reiserfs_fs_is_consistent(src_fs)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL,
	   _("Source filesystem isn't consistent."));
	goto error;
    }

    dst_blocks = dal_len(dst_dal);
    needed_blocks = reiserfs_fs_bitmap_used(src_fs);
	
    if (dst_blocks < needed_blocks) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL,
	    _("Device is too small for (%lu) blocks."), needed_blocks);
	goto error;
    }

    memset(label, 0, sizeof(label));
    memset(uuid, 0, sizeof(uuid));

    /* Creating empty filesystem on given device */
    if (!(dst_fs = reiserfs_fs_create(dst_dal, dst_dal, 0, 
	    get_jp_max_trans_len(get_sb_jp(src_fs->super)), 
	    get_jp_len(get_sb_jp(src_fs->super)), get_sb_block_size(src_fs->super), 
	    get_sb_format(src_fs->super), get_sb_hash_code(src_fs->super), label, 
	    uuid, dal_len(dst_dal))))
	goto error;
   
    if (!reiserfs_segment_init(&src_segment, src_fs->dal, src_fs->super_off + 2 + 
	    reiserfs_fs_journal_area(src_fs), reiserfs_fs_size(src_fs)))
	goto error;
	
    if (!reiserfs_segment_init(&dst_segment, dst_fs->dal, dst_fs->super_off + 2 + 
	    reiserfs_fs_journal_area(dst_fs), reiserfs_fs_size(dst_fs)))
	goto error;
	
    /* Marking dst_fs as corrupted */
    if (!reiserfs_fs_state_update(dst_fs, FS_CORRUPTED))
	goto error_free_dst_fs;
	
    if ((gauge = libreiserfs_get_gauge())) {
	libreiserfs_gauge_reset(gauge);
	libreiserfs_gauge_set_name(gauge, _("copying"));
    }

    reiserfs_fs_bitmap_unuse_block(dst_fs, get_sb_root_block(dst_fs->super));
	
    if (!(root_blk = reiserfs_segment_relocate(dst_fs, &dst_segment, 
	    src_fs, &src_segment, 0)))
	goto error_free_dst_fs;    
	
    if (gauge)
	libreiserfs_gauge_finish(gauge, 1);

    set_sb_root_block(dst_fs->super, root_blk);
	
    set_sb_free_blocks (dst_fs->super, reiserfs_fs_bitmap_unused(dst_fs));
    dst_fs->super->s_v1.sb_tree_height = src_fs->super->s_v1.sb_tree_height;
	
    /* Marking dst_fs as consistent */
    if (!reiserfs_fs_state_update(dst_fs, FS_CONSISTENT))
	goto error_free_dst_fs;

    return dst_fs;

error_free_dst_fs:
    reiserfs_fs_close(dst_fs);    
error:
    return NULL;    
}    

char *reiserfs_fs_label(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return NULL);
    return (reiserfs_fs_format(fs) == FS_FORMAT_3_6 ? fs->super->s_label : NULL);
}

void reiserfs_fs_label_update(reiserfs_fs_t *fs, const char *label) {
    ASSERT(fs != NULL, return);

    if (reiserfs_fs_format(fs) == FS_FORMAT_3_5)
	return;
	
    if (label) {
	int label_len = (strlen(label) < sizeof(fs->super->s_label) ? strlen(label) : 
	    sizeof(fs->super->s_label) - 1);
	
	memcpy(fs->super->s_label, label, label_len);
    } else
	memset(fs->super->s_label, 0, sizeof(fs->super->s_label));

    reiserfs_fs_mark_super_dirty(fs);	
}

char *reiserfs_fs_uuid(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return NULL);
    return (reiserfs_fs_format(fs) == FS_FORMAT_3_6 ? fs->super->s_uuid : NULL);
}

void reiserfs_fs_uuid_update(reiserfs_fs_t *fs, const char *uuid) {
    ASSERT(fs != NULL, return);
	
    if (reiserfs_fs_format(fs) == FS_FORMAT_3_5)
	return;
	
    if (uuid) {
	int uuid_len = (strlen(uuid) < sizeof(fs->super->s_uuid) ? strlen(uuid) : 
	    sizeof(fs->super->s_uuid) - 1);
	
	memcpy(fs->super->s_uuid, uuid, uuid_len);
    } else
	memset(fs->super->s_uuid, 0, sizeof(fs->super->s_uuid));
	
    reiserfs_fs_mark_super_dirty(fs);	
}

dal_t *reiserfs_fs_host_dal(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return NULL);
    return fs->dal;
}

/* Format functions */
static char *reiserfs_long_formats[] = {"reiserfs 3.5", "unknown", "reiserfs 3.6"};
static char *reiserfs_short_formats[] = {"3.5", "unknown", "3.6"};

int reiserfs_fs_format(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return -1);
    return get_sb_format(fs->super);
}

const char *reiserfs_fs_long_format_str(int format) {
    if (format < 0 || format > 2)
	return NULL;

    return reiserfs_long_formats[format];
}

const char *reiserfs_fs_short_format_str(int format) {
    if (format < 0 || format > 2)
	return NULL;

    return reiserfs_short_formats[format];
}

int reiserfs_fs_format_from_str(const char *format) {
    int i;    

    ASSERT(format != NULL, return -1);
	
    for (i = 0; i < 3; i++) {
	if (!strcmp(reiserfs_short_formats[i], format))
	    return i;
    }
    return -1;
}

/* Hashes */
static char *reiserfs_hashes[] = {"unknown", "tea", "yura", "r5"};

reiserfs_hash_t reiserfs_fs_hash(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
    return get_sb_hash_code(fs->super);
}

const char *reiserfs_fs_hash_str(reiserfs_hash_t hash) {
    if (hash > 4) return NULL;
    return reiserfs_hashes[hash];
}

reiserfs_hash_t reiserfs_fs_hash_from_str(const char *hash) {
    int i;
	
    ASSERT(hash != NULL, return 0);
    for (i = 0; i < 4; i++) {
    	if (!strcmp(reiserfs_hashes[i], hash))
	   return (reiserfs_hash_t)i;
    }
    return 0;
}

static char *filesystem_states[] = {"consistent", "corrupted"};

uint32_t reiserfs_fs_fs_state(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
    return get_sb_fs_state(fs->super);
}

int reiserfs_fs_state_update(reiserfs_fs_t *fs, uint32_t state) {
    ASSERT(fs != NULL, return 0);
	
    set_sb_fs_state(fs->super, state);
	
    return reiserfs_fs_super_sync(fs);
}

const char *reiserfs_fs_state_str(uint32_t state) {
    if (state > 1) return NULL;
    return filesystem_states[state];
}

size_t reiserfs_fs_block_size(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
    return get_sb_block_size(fs->super);
}

blk_t reiserfs_fs_size(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
    return get_sb_block_count(fs->super);
}

blk_t reiserfs_fs_min_size(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
    return get_sb_block_count(fs->super) - get_sb_free_blocks(fs->super) - 
	(get_sb_free_blocks(fs->super) / (8 * reiserfs_fs_block_size(fs)));
}

blk_t reiserfs_fs_free_size(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
    return get_sb_free_blocks(fs->super);
}

blk_t reiserfs_fs_metadata_size(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
    return fs->super_off + get_sb_bmap_nr(fs->super) + reiserfs_fs_journal_area(fs);
}

uint32_t reiserfs_fs_tree_height(reiserfs_fs_t *fs) {
    ASSERT(fs != NULL, return 0);
    return get_sb_tree_height(fs->super);
}

