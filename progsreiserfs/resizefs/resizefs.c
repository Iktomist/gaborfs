/*
    resizefs.c -- reiserfs filesystem resize program.
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

static void resizefs_print_usage(void) {
    fprintf(stderr, _("Usage: resizefs.reiserfs [ options ] DEV [+|-]size[K|M|G]\n"
    "Usage: resizefs.reiserfs [ options ] FILE start[K|M|G] end[K|M|G]\n"
	"Options:\n"
	"  -v | --version                  prints current version\n"
	"  -u | --usage                    prints program usage\n"
	"  -j FILE | --journal-device=FILE journal device for separated journal\n"
	"  -n | --no-journal-available     no journal device available now\n"
	"  -f | --force                    force resizer to resize partition anyway\n"
	"  -q | --quiet                    non-interactive mode\n"));
}

int main(int argc, char *argv[]) {
    long dev_len;
    long start = 0, end = 0;
	
    int quiet = 0, force = 0; 
    int choice = 0, journal = 1, error = 0;
	
    dal_t *host_dal = NULL, *journal_dal = NULL;
    char *host_dev = NULL, *journal_dev = NULL;
    char *start_str = NULL, *end_str = NULL;

    reiserfs_fs_t *fs;
    reiserfs_gauge_t *gauge = NULL;
	
    static struct option long_options[] = {
	{"version", no_argument, NULL, 'v'},
	{"usage", no_argument, NULL, 'u'},
	{"journal-device", required_argument, NULL, 'j'},
	{"no-journal-available", required_argument, NULL, 'n'},
	{"force", no_argument, NULL, 'f'},
	{"quiet", no_argument, NULL, 'q'},
	{0, 0, 0, 0}
    };

#ifdef ENABLE_NLS
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif
	
    while ((choice = getopt_long_only(argc, argv, "-uvj:nqf0123456789KMG", 
	long_options, (int *)0)) != EOF) 
    {
	switch (choice) {
	    case 'u': {
		resizefs_print_usage();
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
	    case 'q': {
	    	quiet = 1;
		break;
	    }
	    case 'f': {
		force = 1;
		break;
	    }
	    case 'j': {
		if (!progs_dev_check((journal_dev = optarg))) {
		    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
			_("Device %s doesn't exists or invalid."), journal_dev);
		    return 0;
		}
	    }
	    case '0': case '1':	case '2': case '3':	case '4':
	    case '5': case '6': case '7': case '8':	case '9': 
	    case 'K': case 'M':	case 'G': break;
										  
	    case '?': {
		resizefs_print_usage();
		return 0xfe;
	    }
	}
    }

    choice = argc - 1;
    end_str = argv[choice--];
	
    if (!progs_digit_check(end_str)) {
	resizefs_print_usage();
	return 0xfe;
    }
	
    start_str = argv[choice--];

    if (!progs_digit_check(start_str)) {
	libreiserfs_exception_fetch_all();
	if (progs_dev_check(start_str)) {
	    libreiserfs_exception_leave_all();
	    host_dev = start_str;
	    start_str = NULL;
	} else {
	    libreiserfs_exception_leave_all();
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		_("Device %s doesn't exists or invalid."), start_str);
	    return 0xfe;
	}
    } else
    	host_dev = argv[choice];
	
    /* Checking given device for validness */
    if (!progs_dev_check(host_dev)) {
        libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	   _("Device %s doesn't exists or invalid."), host_dev);
	return 0xfe;
    }
	
    /* Creating device abstraction layer */
    if (!(host_dal = file_open(host_dev, DEFAULT_BLOCK_SIZE, 
        O_RDWR))) 
    {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't open device %s. %s."), host_dev, strerror(errno));
    	goto error;
    }

    if (journal_dev) {
    	if (!strcmp(journal_dev, host_dev))
	   journal_dal = NULL;
	else {
	    if (!(journal_dal = file_open(journal_dev, dal_get_blocksize(host_dal),
		O_RDONLY))) 
	    {
		libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		    _("Couldn't open device %s. %s."), journal_dev, strerror(errno));
		goto error_free_host_dal;
	    }
	}    
    }

    if (!(fs = reiserfs_fs_open(host_dal, 
	(!journal ? NULL : (journal_dal ? journal_dal : host_dal))))) 
    {
    	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't open reiserfs on device %s."), host_dev);
	goto error_free_journal_dal;
    }

    if ((start_str && !(start = progs_digit_parse(start_str, 
	reiserfs_fs_block_size(fs), &error)) && error) || start < 0) 
    {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Invalid \"start\" modificator (%s)."), start_str);
	goto error_free_fs;
    }
   
    if (!(end = progs_digit_parse(end_str, 
	reiserfs_fs_block_size(fs), &error)) && error) 
    {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Invalid \"end\" modificator (%s)."), end_str);
	goto error_free_fs;
    }
	
    if (end_str[0] == '-' || end_str[0] == '+')
	end += reiserfs_fs_size(fs);

    if (end < 0) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Invalid filesystem size (%d)."), end_str);
	goto error_free_fs;
    }

    dev_len = dal_len(host_dal);

    if (force) {
	if (end > dev_len)
	    end = dev_len;

	if ((blk_t)end < reiserfs_fs_min_size(fs))
	    end = reiserfs_fs_min_size(fs);
    } else {
	if (end > dev_len) {
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		_("Can't resize filesystem outside the device."));
	    goto error_free_fs;
	}
    }
	
    choice = 'y';
    if (!quiet) {
	if (!(choice = progs_choose("ynYN", _("Please select (y/n) "), 
		_("Are you ready (y/n) "))))
	    goto error_free_fs;
    }
	
    if (choice == 'n' || choice == 'N')
	goto error_free_fs;

    fprintf(stderr, _("Resizing %s\n"), host_dev);
    fflush(stderr);

    if (!(gauge = libreiserfs_gauge_create(REISERFS_GAUGE_PERCENTAGE, NULL, NULL)))
	goto error_free_fs;
    
    libreiserfs_set_gauge(gauge);
	
    if (!(start == 0 ? reiserfs_fs_resize_dumb(fs, end) : 
	reiserfs_fs_resize_smart(fs, start, end))) 
    {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't resize filesystem on %s to %lu - %lu blocks."), host_dev, 
	    start, end);
	goto error_free_gauge;
    }
	
    libreiserfs_gauge_free(gauge);
    libreiserfs_set_gauge(NULL);
	
    reiserfs_fs_close(fs);

    if (!(gauge = libreiserfs_gauge_create(REISERFS_GAUGE_SILENT, _("syncing"), NULL)))
	goto error_free_journal_dal;

    libreiserfs_set_gauge(gauge);
	    
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
error_free_journal_dal:
    if (journal_dal)
	file_close(journal_dal);
error_free_host_dal:
    file_close(host_dal);
error:
    return 0xff;
}

