/*
    cpfs.c -- reiserfs filesystem copy program.
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "getopt.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
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

static void cpfs_print_usage(void) {
    fprintf(stderr, _("Usage: cpfs.reiserfs [ options ] SRC DEST\n"
    	"Options:\n"
    	"  -v | --version                  prints current version\n"
    	"  -u | --usage                    prints program usage\n"
    	"  -j FILE | --journal-device=FILE journal device for separated journal\n"
    	"  -n | --no-journal-available     no journal device available now\n"
    	"  -q | --quiet                    non-interactive mode\n"));
}

int main(int argc, char *argv[]) {
    reiserfs_fs_t *src_fs, *dst_fs;
    reiserfs_gauge_t *gauge = NULL;
    int error = 0, choice = 0, quiet = 0, journal = 1;
	
    dal_t *src_host_dal, *dst_host_dal = NULL;
    dal_t *src_journal_dal = NULL;
	
    char *src_host_dev = NULL, *dst_host_dev = NULL;
    char *src_journal_dev = NULL;
	
    static struct option long_options[] = {
    	{"version", no_argument, NULL, 'v'},
	{"usage", no_argument, NULL, 'u'},
	{"journal-device", required_argument, NULL, 'j'},
	{"no-journal-available", no_argument, NULL, 'n'},
	{"quiet", no_argument, NULL, 'q'},
	{0, 0, 0, 0}
    };
	
    while ((choice = getopt_long_only(argc, argv, "uvj:nq", long_options, 
	(int *)0)) != EOF) 
    {
	switch (choice) {
	    case 'u': {
		cpfs_print_usage();
		return 0;
	    }
	    case 'v': {
		printf("%s %s\n", argv[0], VERSION);
		return 0;
	    }
	    case 'j': {
		if (!progs_dev_check(src_journal_dev = optarg)) {
		    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
			_("Device %s doesn't exists or invalid."), optarg);
		    return 0xfe;	
		}
		break;
	    }
	    case 'n': {
		journal = 0;
		break;
	    }
	    case 'q': {
		quiet = 1;
		break;
	    }
	    case '?': {
		cpfs_print_usage();
		return 0xfe;
	    }
	}
    }
	
    if (optind >= argc || optind + 1 >= argc) {
    	cpfs_print_usage();
	return 0xfe;
    }

    src_host_dev = argv[optind++];
    dst_host_dev = argv[optind];
	
    if (!strcmp(src_host_dev, dst_host_dev)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Source and destination devices are equal."));
	return 0xfe;
    }
	
    /* Checking given device for validness */
    if (!progs_dev_check(src_host_dev)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Device %s doesn't exists or invalid."), src_host_dev);
	return 0xfe;
    }
	
    if (!progs_dev_check(dst_host_dev)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Device %s doesn't exists or invalid."), dst_host_dev);
	return 0xfe;
    }

    /* Creating source device abstraction handler */
    if (!(src_host_dal = file_open(src_host_dev, DEFAULT_BLOCK_SIZE,
	O_RDONLY))) 
    {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't open device %s. %s."), src_host_dev, strerror(errno));
	goto error;
    }

    if (src_journal_dev) {
	if (!strcmp(src_journal_dev, dst_host_dev)) {
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		_("Source filesystem journal device and destination device are equal."));
	    goto error;
	}
	
	if (!strcmp(src_journal_dev, src_host_dev))
	    src_journal_dal = NULL;
	else {
	    if (!(src_journal_dal = file_open(src_journal_dev, 
		dal_get_blocksize(src_host_dal), O_RDONLY))) 
	    {
		libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		   _("Couldn't open device for source journal %s. %s."), 
		   src_journal_dev, strerror(errno));
		goto error_free_src_host_dal;
	    }
	}    
    }

    if (!(src_fs = reiserfs_fs_open(src_host_dal, 
	(!journal ? NULL : (src_journal_dal ? src_journal_dal : src_host_dal))))) 
    {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't open reiserfs on device %s."), src_host_dev);
	goto error_free_src_host_dal;
    }

    if (!(dst_host_dal = file_open(dst_host_dev, dal_get_blocksize(src_host_dal),
    	O_RDWR))) 
    {
    	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't open device %s. %s."), dst_host_dev, strerror(errno));
	goto error_free_src_fs;
    }

    choice = 'y';
    if (!quiet) {
	if (!(choice = progs_choose("ynYN", _("Please select (y/n) "), 
		_("All data on %s will be lost. Do you realy want to copy "
		"%s to %s (y/n) "),	dst_host_dev, src_host_dev, dst_host_dev)))
	    goto error_free_dst_host_dal;
    }
	
    if (choice == 'n' || choice == 'N')
	goto error_free_dst_host_dal;
	
    fprintf(stderr, _("Copying %s to %s\n"), src_host_dev, dst_host_dev);
    fflush(stderr);

    if (!(gauge = libreiserfs_gauge_create(REISERFS_GAUGE_PERCENTAGE, NULL, NULL)))
	goto error_free_dst_host_dal;

    libreiserfs_set_gauge(gauge);
    
    if (!(dst_fs = reiserfs_fs_copy(src_fs, dst_host_dal))) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't copy %s to %s."), src_host_dev, dst_host_dev);
	goto error_free_gauge;
    }
	
    libreiserfs_gauge_free(gauge);
    libreiserfs_set_gauge(NULL);
	
    if (!(gauge = libreiserfs_gauge_create(REISERFS_GAUGE_SILENT, _("syncing"), NULL)))
	goto error_free_dst_host_dal;

    libreiserfs_set_gauge(gauge);

    reiserfs_fs_close(dst_fs);
    reiserfs_fs_close(src_fs);

    if (!dal_sync(dst_host_dal)) {
	libreiserfs_exception_throw(EXCEPTION_WARNING, EXCEPTION_OK, 
	    "Can't synchronize device %s. %s.", dal_name(dst_host_dal), 
	    dal_error(dst_host_dal));
    }
    
    file_close(dst_host_dal);

    libreiserfs_gauge_finish(gauge, 1);
    libreiserfs_gauge_free(gauge);
    libreiserfs_set_gauge(NULL);
    
    if (src_journal_dal) 
	file_close(src_journal_dal);
	
    file_close(src_host_dal);
    return 0;
	
error_free_gauge:
    libreiserfs_gauge_free(gauge);
    libreiserfs_set_gauge(NULL);
error_free_dst_host_dal:
    file_close(dst_host_dal);
error_free_src_fs:
    reiserfs_fs_close(src_fs);    
error_free_src_journal_dal:
    if (src_journal_dal)
    	file_close(src_journal_dal);
error_free_src_host_dal:
    file_close(src_host_dal);
error:
    return 0xff;
}

