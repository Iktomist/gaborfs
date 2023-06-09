/*
    tunefs.c -- reiserfs filesystem tune program.
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "getopt.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <dal/file.h>

#include <reiserfs/reiserfs.h>
#include <reiserfs/libprogs_tools.h>

#if ENABLE_NLS
#  include <locale.h>
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif

static void tunefs_print_usage(void) {
    fprintf(stderr, _("Usage: tunefs.reiserfs options device\n"));
    fprintf(stderr, _("Options:\n"
	"  -v | --version                      prints current version\n"
	"  -u | --usage                        prints program usage\n"
	"  -j FILE | --journal-device=FILE     device where journal is lies\n"
	"  -d FILE | --journal-new-device=FILE new journal device\n"
	"  -s N | --journal-size=N             journal size\n"
	"  -o N | --journal-offset=N           journal offset for relocated journal\n"
	"  -t N | --transaction-max-size=N     transaction max size\n"
	"  -n | --no-journal-available         no journal available now\n"));
    fprintf(stderr, _(
	"  -l LABEL | --label=LABEL            sets volume label\n"
	"  -i UUID | --uuid=UUID               sets given uuid to superblock\n"
	"  -q | --quiet                        non-interactive mode\n"));
}

int main(int argc, char *argv[]) {
    int need_tuning = 1, error = 0;
    int choice, quiet = 0, journal = 1;
    int new_journal_relocated, old_journal_relocated;
	
    long len = 0, start = 0;
    long max_trans_size = 0;

    reiserfs_fs_t *fs;
    reiserfs_gauge_t *gauge = NULL;
	
    dal_t *host_dal = NULL, *journal_dal = NULL, *new_journal_dal = NULL;
    char *host_dev = NULL, *journal_dev = NULL, *new_journal_dev = NULL;
    char *label = NULL, *uuid = NULL;
	
    static struct option long_options[] = {
	{"version", no_argument, NULL, 'v'},
	{"usage", no_argument, NULL, 'u'},
	{"journal-device", required_argument, NULL, 'j'},
	{"journal-new-device", required_argument, NULL, 'd'},
	{"journal-size", required_argument, NULL, 's'},
	{"journal-offset", required_argument, NULL, 'o'},
	{"transaction-max-size", required_argument, NULL, 't'},
	{"no-journal-available", required_argument, NULL, 'n'},
	{"label", required_argument, NULL, 'l'},
	{"uuid", required_argument, NULL, 'i'},
	{"quiet", no_argument, NULL, 'q'},
	{0, 0, 0, 0}
    };
	
#ifdef ENABLE_NLS
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif
	
    while ((choice = getopt_long_only(argc, argv, "uvj:d:s:o:t:nl:i:q", long_options, 
	(int *)0)) != EOF) 
    {
	switch (choice) {
	    case 'u': {
		tunefs_print_usage();
		return 0;
	    }
	    case 'v': {
		printf("%s %s\n", argv[0], VERSION);
		return 0;
	    }
	    case 'n': {
		journal = 0;
		break;
	    }
	    case 'j': {
		if (!progs_dev_check(journal_dev = optarg)) {
		    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
			_("Device %s doesn't exists or invalid."), optarg);
		    return 0xfe;
		}
		break;
	    }
	    case 'd': {
		if (!progs_dev_check(new_journal_dev = optarg)) {
		    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
			_("Device %s doesn't exists or invalid."), optarg);
		    return 0xfe;
		}
		break;
	    }
	    case 's': {
		if (!(len = progs_strtol(optarg, &error)) && error) {
		    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
			_("Invalid journal size (%s)."), optarg);
		    return 0xfe;	
		}
		break;
	    }
	    case 'o': {
		if (!(start = progs_strtol(optarg, &error)) && error) {
		    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
			_("Invalid journal offset (%s)."), optarg);
		    return 0xfe;	
		}
		break;
	    }
	    case 't': {
		if (!(max_trans_size = progs_strtol(optarg, &error)) && error) {
		    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
			_("Invalid transaction max size (%s)."), optarg);
		    return 0xfe;	
		}
		break;
	    }
	    case 'i': {
		if (strlen(optarg) < 16) {
		    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
			_("Invalid uuid (%s)."), optarg);
		    return 0xfe;
		}
		uuid = optarg;
		break;
	    }
	    case 'l': {
		label = optarg;
		break;
	    }
	    case 'q': {
		quiet = 1;
		break;
	    }
	    case '?': {
		tunefs_print_usage();
		return 0xfe;
	    }
	}
    }
	
    if (optind >= argc) {
	tunefs_print_usage();
	return 0xfe;
    }
	
    host_dev = argv[optind];
	
    /* Checking given device for validness */
    if (!progs_dev_check(host_dev)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Device %s doesn't exists or invalid."), host_dev);
	return 0xfe;
    }
	
    /* Creating device abstraction layer */
    if (!(host_dal = file_open(host_dev, DEFAULT_BLOCK_SIZE, O_RDWR))) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't open device %s. %s."), host_dev, strerror(errno));
	goto error;
    }

    if (journal_dev) {
	if (!strcmp(journal_dev, host_dev))
	    journal_dal = NULL;
	else {
	    if (!(journal_dal = file_open(journal_dev, dal_get_blocksize(host_dal), 
		O_RDWR)))
	    {
		libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		    _("Couldn't open device %s. %s."), journal_dev, strerror(errno));
		goto error_free_host_dal;
	    }
	}    
    }

    if (new_journal_dev) {
	if ((journal_dev && !strcmp(new_journal_dev, journal_dev)) || 
		!strcmp(new_journal_dev, host_dev))
	    new_journal_dal = NULL; 
	else {
	    if (!(new_journal_dal = file_open(new_journal_dev, 
		dal_get_blocksize(host_dal), O_RDWR))) 
	    {
		libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		    _("Couldn't open device %s. %s."), new_journal_dev, strerror(errno));
		goto error_free_journal_dal;
	    }
	}
    }

    if (!(fs = reiserfs_fs_open(host_dal, (!journal ? NULL : 
	(journal_dal ? journal_dal : host_dal))))) 
    {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't open reiserfs on device %s."), host_dev);
	goto error_free_journal_dal;
    }
	
    old_journal_relocated = journal_dev && strcmp(journal_dev, host_dev);
	
    if (old_journal_relocated != reiserfs_fs_journal_relocated(fs)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Invalid journal location parameters detected. Filesystem "
	    "has %s journal, but specified %s journal."), 
	    reiserfs_fs_journal_kind_str(reiserfs_fs_journal_relocated(fs)), 
	    reiserfs_fs_journal_kind_str(old_journal_relocated));
	goto error_free_journal_dal;
    }	
	
    new_journal_relocated = new_journal_dev && strcmp(new_journal_dev, host_dev);
    need_tuning = reiserfs_fs_journal_relocated(fs) != new_journal_relocated ||
	max_trans_size || start || len;
	
    if (!need_tuning && !label && !uuid) {
	libreiserfs_exception_throw(EXCEPTION_INFORMATION, EXCEPTION_CANCEL, 
	    _("Filesystem doesn't needed tunning."));
	goto error_free_journal_dal;
    }
	
    choice = 'y';
    if (!quiet) {
	if (!(choice = progs_choose("ynYN", _("Please select (y/n) "), 
		_("Are you ready (y/n) "))))
	    goto error_free_journal_dal;
    }
	
    if (choice == 'n' || choice == 'N')
	goto error_free_journal_dal;

    if (!max_trans_size) {
	max_trans_size = JOURNAL_MAX_TRANS;
        if (reiserfs_fs_block_size(fs) < 4096)
	    max_trans_size = JOURNAL_MAX_TRANS /(4096 / reiserfs_fs_block_size(fs));
    }

    if (new_journal_dal) {
	if (!len)
	    len = reiserfs_journal_max_len(new_journal_dal, start, 1);
    } else {
	blk_t max_journal_len;
		
	if (!start)
	    start = reiserfs_fs_journal_offset(fs);
		
	if (!len) 
	    len = dal_get_blocksize(host_dal) == 1024 ? DEFAULT_JOURNAL_SIZE_BS1024 :
		DEFAULT_JOURNAL_SIZE_BS4096;

	if ((blk_t)len > (max_journal_len = reiserfs_journal_max_len(host_dal, start, 0)))
	    len = max_journal_len;
    }

    fprintf(stderr, _("Tunning %s\n"), host_dev);
    fflush(stderr);

    if (need_tuning) {
    	if (!(gauge = libreiserfs_gauge_create(REISERFS_GAUGE_PERCENTAGE, NULL, NULL)))
	    goto error_free_fs;
	    
	libreiserfs_set_gauge(gauge);
		
	if (!reiserfs_fs_journal_tune(fs, new_journal_dal ? new_journal_dal : host_dal, 
	    start, len, max_trans_size)) 
	{
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		_("Couldn't tune filesystem."));
	    goto error_free_gauge;
	}
		
	libreiserfs_gauge_free(gauge);
        libreiserfs_set_gauge(NULL);
    }
	
    if ((uuid || label) && reiserfs_fs_format(fs) == FS_FORMAT_3_5) {
	libreiserfs_exception_throw(EXCEPTION_INFORMATION, EXCEPTION_CANCEL, 
	    _("Sorry, label and uuid not supported for reiserfs 3.5."));
	goto error_free_fs;
    }
	
    /* Updating uuid and label */
    if (uuid)
    	reiserfs_fs_uuid_update(fs, uuid);
	
    if (label)
    	reiserfs_fs_label_update(fs, label);
	
    reiserfs_fs_close(fs);
	
    if (!(gauge = libreiserfs_gauge_create(REISERFS_GAUGE_SILENT, _("syncing"), NULL)))
	goto error_free_new_journal_dal;
	    
    libreiserfs_set_gauge(gauge);
	
    if (new_journal_dal) {
    	if (!dal_sync(new_journal_dal)) {
	    libreiserfs_exception_throw(EXCEPTION_WARNING, EXCEPTION_OK, 
		"Can't synchronize device %s. %s.", 
		dal_name(new_journal_dal), dal_error(new_journal_dal));
	}
    	file_close(new_journal_dal);
    }
	
    if (journal_dal) {
    	if (!dal_sync(journal_dal)) {
	    libreiserfs_exception_throw(EXCEPTION_WARNING, EXCEPTION_OK, 
		"Can't synchronize device %s. %s.", 
		dal_name(journal_dal), dal_error(journal_dal));
	}
	file_close(journal_dal);
    }

    if (!dal_sync(host_dal)) {
	libreiserfs_exception_throw(EXCEPTION_WARNING, EXCEPTION_OK, 
	    "Can't synchronize device %s. %s.", 
	    dal_name(host_dal), dal_error(host_dal));
    }
    file_close(host_dal);

    libreiserfs_gauge_finish(gauge, 1);
    libreiserfs_gauge_free(gauge);
    libreiserfs_set_gauge(NULL);

    return 0;
	
error_free_gauge:
    libreiserfs_gauge_free(gauge);
    libreiserfs_set_gauge(NULL);
error_free_fs:
    reiserfs_fs_close(fs);    
error_free_new_journal_dal:
    if (new_journal_dal)
    	file_close(new_journal_dal);    
error_free_journal_dal:
    if (journal_dal)
    	file_close(journal_dal);
error_free_host_dal:
    file_close(host_dal);
error:
    return 0xff;
}
