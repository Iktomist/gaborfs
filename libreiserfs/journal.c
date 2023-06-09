/*
    journal.c -- reiserfs journal code
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.

    Some parts of this code inspired by the original reiserfs code, as found in 
    reiserfsprogs and the linux kernel.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <reiserfs/reiserfs.h>
#include <reiserfs/debug.h>
#include <reiserfs/callback.h>

#include <dal/dal.h>

#define N_(String) (String)
#if ENABLE_NLS
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif

#define JOURNAL_DESC_SIGN	"ReIsErLB"
#define JOURNAL_DESC_COMM	1
#define JOURNAL_DESC_NEXT 	2

#define CASHE_SIZE(len) ((len) * sizeof(uint32_t))

static int reiserfs_journal_desc_match_comm(reiserfs_block_t *desc, reiserfs_block_t *comm) {
    return (get_jc_commit_trans_id(comm) == get_jd_desc_trans_id(desc) &&
	get_jc_commit_trans_len(comm) == get_jd_desc_trans_len(desc));
}

static blk_t reiserfs_journal_desc_prop(reiserfs_journal_head_t *head, reiserfs_block_t *desc, 
    int prop) 
{
    blk_t offset = reiserfs_block_get_nr(desc) - get_jp_start(&head->jh_params);
    return get_jp_start(&head->jh_params) + ((offset + 
	get_jd_desc_trans_len(desc) + prop) % get_jp_len(&head->jh_params));
}

static blk_t reiserfs_journal_desc_comm(reiserfs_journal_head_t *head, reiserfs_block_t *desc) {
    return reiserfs_journal_desc_prop(head, desc, JOURNAL_DESC_COMM);
}

static blk_t reiserfs_journal_desc_next(reiserfs_journal_head_t *head, reiserfs_block_t *desc) {
    return reiserfs_journal_desc_prop(head, desc, JOURNAL_DESC_NEXT);
}

static int reiserfs_journal_desc_block(reiserfs_block_t *desc) {
    if (!memcmp(get_jd_magic(desc, dal_get_blocksize(desc->dal)), JOURNAL_DESC_SIGN, 8) && 
	    LE32_TO_CPU(*((uint32_t *)(desc->data + 4))) > 0)
	return 1;
	
    return 0;
}

/* 
    Checking whether given transaction is valid. Transaction is 
    valid if his description block is valid, commit block is valid 
    and commit block matches description block.
	
    Transaction looks like this:
    desc_block + [ trans_blocks ] + comm_block
*/
static int reiserfs_journal_desc_valid(reiserfs_journal_head_t *head, 
    reiserfs_block_t *desc)
{
    blk_t blk;
    reiserfs_block_t *comm;
	
    if (!desc || !reiserfs_journal_desc_block(desc))
	return 0;

    blk = reiserfs_journal_desc_comm(head, desc);
    if (!(comm = reiserfs_block_read(desc->dal, blk)))
	reiserfs_block_reading_failed(blk, dal_error(desc->dal), return 0);
	
    if (!reiserfs_journal_desc_match_comm(desc, comm))
	goto error_free_comm;
	
    reiserfs_block_free(comm);
    return 1;
	
error_free_comm:
    reiserfs_block_free(comm);
error:
    return 0;
}

uint32_t reiserfs_journal_max_trans(blk_t max_trans, blk_t len, 
    size_t blocksize) 
{
    uint32_t ratio = 1;

    if (blocksize < 4096)
	ratio = 4096 / blocksize;
	
    if (!max_trans)
	max_trans = JOURNAL_MAX_TRANS / ratio;
	
    if (len / max_trans < JOURNAL_MIN_RATIO)
	max_trans = len / JOURNAL_MIN_RATIO;
	
    if (max_trans > JOURNAL_MAX_TRANS / ratio)
    	max_trans = JOURNAL_MAX_TRANS / ratio;
	
    if (max_trans < JOURNAL_MIN_TRANS / ratio)
	max_trans = JOURNAL_MIN_TRANS / ratio;
	
    return max_trans;
}

blk_t reiserfs_journal_max_len(dal_t *dal, blk_t start, int relocated) {
    if (relocated)
	return dal_len(dal) - start - 1;
	
    return (dal_get_blocksize(dal) * 8) - start - 1;
}

int reiserfs_journal_params_check(dal_t *dal, blk_t start, blk_t len, int relocated) {
    blk_t max_len;
	
    ASSERT(dal != NULL, return 0);

    if (!relocated) {
	blk_t super_blk = (DEFAULT_SUPER_OFFSET / dal_get_blocksize(dal));
	
    	if (start && start != super_blk + 2) {
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		_("Invalid journal start (%lu) for journal on host device."), start);
	    return 0;	
	}
    }
	
   max_len = reiserfs_journal_max_len(dal, start, relocated);
	
    if (len > max_len) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Journal size is too big (%lu). It must be smaller or equal "
	    "%lu blocks for block size %d."), len, max_len, dal_get_blocksize(dal));
	return 0;
    }
	
    if (len && len < JOURNAL_MIN_SIZE) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Journal size (%lu) is smaller minimal recomended (%lu)."), 
	    len, JOURNAL_MIN_SIZE);
	return 0;
    }
	
    return 1;
}    

void reiserfs_journal_params_update(reiserfs_journal_params_t *params, blk_t start, 
    blk_t len, blk_t max_trans, uint32_t dev, size_t blocksize)
{
	
    ASSERT(params != NULL, return);
	
    set_jp_start(params, start);
    set_jp_len(params, len);
	
    set_jp_max_trans_len(params, 
	reiserfs_journal_max_trans(max_trans, len, blocksize));
	
    set_jp_magic(params, reiserfs_tools_random());
    set_jp_max_batch(params, max_trans * JOURNAL_MAX_BATCH/JOURNAL_MAX_TRANS);
	
    set_jp_max_commit_age(params, JOURNAL_MAX_COMMIT_AGE);
    set_jp_max_trans_age(params, JOURNAL_MAX_TRANS_AGE);
	
    set_jp_dev(params, dev);
}

int reiserfs_journal_pipe(reiserfs_journal_t *journal, blk_t from,
    reiserfs_journal_pipe_func_t pipe_func, void *data)
{
    blk_t start, len, curr;
    reiserfs_block_t *desc, *comm;
    blk_t replay_start = get_jh_replay_offset(&journal->head);
	
    start = get_jp_start(&journal->head.jh_params);
    len = get_jp_len(&journal->head.jh_params);

    if (from >= len) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Invalid start (%lu) for journal len %lu."), from, len);
	return 0;
    }
	
    for (curr = from; curr < len; curr++) {
	if (!(desc = reiserfs_block_read(journal->dal, start + curr)))
	    reiserfs_block_reading_failed(start + curr, dal_error(journal->dal), return 0);
	
	if (!(comm = reiserfs_block_read(journal->dal, 
	    reiserfs_journal_desc_comm(&journal->head, desc))))
	{
	    reiserfs_block_reading_failed(reiserfs_journal_desc_comm(&journal->head, desc), 
		dal_error(journal->dal), goto error_free_desc);
	}
	
	if (!reiserfs_journal_desc_valid(&journal->head, desc))
	    goto error_continue;

	if (pipe_func && !pipe_func(journal, desc, comm, curr, data))
	    goto error_free_comm;
		
	curr += get_jd_desc_trans_len(desc) + 1;
error_continue:
	reiserfs_block_free(comm);
	reiserfs_block_free(desc);
    }
    return 1;
	
error_free_comm:
    libreiserfs_free(comm);
error_free_desc:
    libreiserfs_free(desc);
error:
    return 0;
}

struct reiserfs_read_desc {
    blk_t needle, found;
};

static int callback_journal_read(reiserfs_journal_t *journal, reiserfs_block_t *desc, 
    reiserfs_block_t *comm, uint32_t number, struct reiserfs_read_desc *read_desc) 
{
    uint32_t i;
    blk_t trans_half = journal_trans_half(dal_get_blocksize(journal->dal));
    blk_t len = get_jp_len(&journal->head.jh_params) - 1;
    blk_t start = get_jp_start(&journal->head.jh_params);

    for (i = 0; i < get_jd_desc_trans_len(desc) && i < trans_half; i++)	{	
	if (read_desc->needle == LE32_TO_CPU(get_desc_header(desc)->jd_realblock[i]))
	    read_desc->found = (start + number + i + 1) & len;
    }	
	
    if (i >= trans_half) {
	for (; i < get_jd_desc_trans_len(desc); i++) {
	    uint32_t blk = LE32_TO_CPU(get_comm_header(comm)->jc_realblock[i - trans_half]);
	    if (read_desc->needle == blk)
	        read_desc->found = (start + number + i + 1) & len;
	}
    }

    return 1;
}

reiserfs_block_t *reiserfs_journal_read(reiserfs_journal_t *journal, blk_t blk) {
    struct reiserfs_read_desc desc;
	
    ASSERT(journal != NULL, return NULL);

    memset(&desc, 0, sizeof(desc));
    desc.needle = blk;
	
    if (!reiserfs_journal_pipe(journal, get_jh_replay_offset(&journal->head), 
	    (reiserfs_journal_pipe_func_t)callback_journal_read, &desc))
	return NULL;
	
    if (desc.found && desc.found > get_jp_start(&journal->head.jh_params) + 
	    get_jp_len(&journal->head.jh_params) - 1)
	return NULL;

    return desc.found ? reiserfs_block_read(journal->dal, desc.found) : NULL;
}

reiserfs_journal_t *reiserfs_journal_open(dal_t *dal, blk_t start, blk_t len, int relocated) {
    uint32_t dev;
    reiserfs_block_t *block;
    reiserfs_journal_t *journal;
    reiserfs_journal_params_t *params;
	
    ASSERT(dal != NULL, return NULL);
	
    if (!(block = reiserfs_block_read(dal, start + len)))
	reiserfs_block_reading_failed(start + len, dal_error(dal), goto error);
	
    /* Checking for journal validness */
    params = &((reiserfs_journal_head_t *)block->data)->jh_params;
    libreiserfs_exception_fetch_all();
    if (!reiserfs_journal_params_check(dal, get_jp_start(params), 
	get_jp_len(params), relocated))
    {	    
	libreiserfs_exception_leave_all();
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Invalid journal parameters detected."));
	goto error_free_block;
    }	
    libreiserfs_exception_leave_all();
	
    if (get_jh_replay_offset((reiserfs_journal_head_t *)block->data) >= start + len) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Invalid journal parameters detected."));
	goto error_free_block;
    }
	
    if (!(journal = libreiserfs_calloc(sizeof(reiserfs_journal_t), 0)))
    	goto error_free_block;
	
    memcpy(&journal->head, block->data, sizeof(reiserfs_journal_head_t));
	
    if (!(dev = dal_stat(dal))) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't stat journal device."));
	goto error_free_journal;
    }

    set_jp_dev(&journal->head.jh_params, dev);
    reiserfs_block_free(block);

    journal->dal = dal;
	
    return journal;

error_free_journal:
    libreiserfs_free(journal);    
error_free_block:
    reiserfs_block_free(block);
error:
    return NULL;    
}

reiserfs_journal_t *reiserfs_journal_create(dal_t *dal, blk_t start, blk_t len,
	blk_t max_trans, int relocated)
{
    uint32_t dev;
    reiserfs_gauge_t *gauge;
    reiserfs_segment_t segment;
    reiserfs_block_t *block;
    reiserfs_journal_t *journal;
    reiserfs_journal_params_t *params;
	
    ASSERT(dal != NULL, return NULL);
	
    if (!reiserfs_journal_params_check(dal, start, len, relocated))
	return NULL;
	
    if (!reiserfs_segment_init(&segment, dal, start, start + len))
	return NULL;

    if ((gauge = libreiserfs_get_gauge())) {
	libreiserfs_gauge_reset(gauge);
	libreiserfs_gauge_set_name(gauge, _("initializing journal"));
    }

    if (!reiserfs_segment_fill(&segment, 0, (reiserfs_segment_func_t)
	    reiserfs_callback_segment_gauge, gauge))
	return NULL;
		
    if (gauge)
	libreiserfs_gauge_finish(gauge, 1);
	
    dev = 0;
    if (relocated) {
	if (!(dev = dal_stat(dal))) {
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		_("Can't stat journal device."));
	    return NULL;	
	}
    }
	
    if (!(journal = (reiserfs_journal_t *)libreiserfs_calloc(sizeof(*journal), 0)))
	return NULL;
   
    reiserfs_journal_params_update(&journal->head.jh_params, start, len, max_trans, dev, 
	dal_get_blocksize(dal));
	
    if (!(block = reiserfs_block_alloc_with_copy(dal, start + len, 
	    (void *)&journal->head)))
	goto error_free_journal;
	
    /* Writing journal parameters onto device */
    if (!reiserfs_block_write(dal, block)) {
	reiserfs_block_writing_failed(reiserfs_block_get_nr(block), 
	    dal_error(dal), goto error_free_block);
    }
    reiserfs_block_free(block);

    journal->dal = dal;
    return journal;
    
error_free_block:
    reiserfs_block_free(block);
error_free_journal:
    libreiserfs_free(journal);
error:
    return NULL;    
}

void reiserfs_journal_close(reiserfs_journal_t *journal) {
    ASSERT(journal != NULL, return);
	
    if (journal->cashe.blocks) 
	libreiserfs_free(journal->cashe.blocks);

    libreiserfs_free(journal);
}

int reiserfs_journal_sync(reiserfs_journal_t *journal) {
    reiserfs_block_t *block;

    ASSERT(journal != NULL, return 0);
	
    if (!(block = reiserfs_block_alloc_with_copy(journal->dal, 
	    get_jp_start(&journal->head.jh_params) + 
	    get_jp_len(&journal->head.jh_params), (void *)&journal->head)))
	goto error;
	
    if (!reiserfs_block_write(journal->dal, block)) {
	reiserfs_block_writing_failed(reiserfs_block_get_nr(block),
	    dal_error(journal->dal), goto error_free_block);
    }

    reiserfs_block_free(block);
    return 1;
	
error_free_block:
    reiserfs_block_free(block);
error:
    return 0;    
}

static void reiserfs_journal_desc_desc2trans(reiserfs_journal_head_t *head, 
    reiserfs_block_t *desc, reiserfs_journal_trans_t *trans) 
{
    trans->jt_mount_id = get_jd_desc_mount_id(desc);
    trans->jt_trans_id = get_jd_desc_trans_id(desc);
    trans->jt_desc_blocknr = reiserfs_block_get_nr(desc);
    trans->jt_trans_len = get_jd_desc_trans_len(desc);
    trans->jt_commit_blocknr = reiserfs_journal_desc_comm(head, desc);
    trans->jt_next_trans_offset = reiserfs_journal_desc_next(head, desc) - 
	get_jp_len(&head->jh_params);
}

struct reiserfs_replay_desc {
    uint32_t trans;
    reiserfs_gauge_t *gauge;
    uint32_t oldest_id, newest_id;
    reiserfs_journal_trans_t *oldest_tr;
    reiserfs_journal_trans_t *newest_tr;
};

static int callback_journal_replay(reiserfs_journal_t *journal, reiserfs_block_t *desc, 
    reiserfs_block_t *comm, uint32_t number, struct reiserfs_replay_desc *replay_desc) 
{
    (void)comm;

    if (replay_desc->gauge) {
	libreiserfs_gauge_set_value(replay_desc->gauge, (unsigned int)((number * 100) / 
	     get_jp_len(&journal->head.jh_params)) + 1);
    }	

    if (!reiserfs_journal_desc_valid(&journal->head, desc)) {
	reiserfs_block_free(desc);
	return 1;
    }

    replay_desc->trans++;

    if (get_jd_desc_trans_id(desc) < replay_desc->oldest_id) {
	replay_desc->oldest_id = get_jd_desc_trans_id(desc);
	reiserfs_journal_desc_desc2trans(&journal->head, desc, replay_desc->oldest_tr);
    }

    if (get_jd_desc_trans_id(desc) > replay_desc->newest_id) {
	replay_desc->newest_id = get_jd_desc_trans_id(desc);
	reiserfs_journal_desc_desc2trans(&journal->head, desc, replay_desc->newest_tr);
    }
	
    return 1;
}

blk_t reiserfs_journal_boundary_transactions(reiserfs_journal_t *journal,
    reiserfs_journal_trans_t *oldest, reiserfs_journal_trans_t *newest)
{
    reiserfs_gauge_t *gauge=NULL;
    struct reiserfs_replay_desc desc;

    (void)oldest;
    (void)newest;
	
    desc.oldest_id = 0xffffffff; desc.newest_id = 0x0;
	
    if (gauge) {
	libreiserfs_gauge_reset(gauge);
	libreiserfs_gauge_set_name(gauge, _("looking for transactions"));
    }
	
    desc.gauge = gauge; desc.trans = 0;
	
    if (!reiserfs_journal_pipe(journal, 0, 
	    (reiserfs_journal_pipe_func_t)callback_journal_replay, &desc))
	return 0;
	
    if (gauge)
	libreiserfs_gauge_finish(gauge, 1);
	
    return desc.trans;
}

