/*
    fsck.c -- reiserfs filesystem checking and recovering program
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "getopt.h"

#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#include <reiserfs/reiserfs.h>
#include <reiserfs/libprogs_tools.h>

#include <dal/file.h>

#if ENABLE_NLS
#  include <locale.h>
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif

extern long gathering_leaves(reiserfs_block_t *node, void *data);

static void fsck_print_usage(void) {
    fprintf(stderr, _("Usage: fsck.reiserfs [ options ] device\n"
    	"Options:\n"
    	"  -v | --version                  prints current version\n"
    	"  -u | --usage                    prints program usage\n"
	"  -j FILE | --journal-device=FILE journal device for separated journal\n"
	"  -n | --no-journal-available     no journal device available now\n"));
}

int main(int argc, char *argv[]) {
    reiserfs_fs_t *fs;
    int choice, journal = 1;
    dal_t *host_dal, *journal_dal = NULL;
    char *host_dev = NULL, *journal_dev = NULL;
	
    static struct option long_options[] = {
	{"version", no_argument, NULL, 'v'},
	{"usage", no_argument, NULL, 'u'},
	{"journal-device", required_argument, NULL, 'j'},
	{"no-journal-available", no_argument, NULL, 'n'},
	{0, 0, 0, 0}
    };

#ifdef ENABLE_NLS
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif
	
    while ((choice = getopt_long_only(argc, argv, "uvj:n", 
	long_options, (int *)0)) != EOF) 
    {
	switch (choice) {
	    case 'u': {
		fsck_print_usage();
		return 0;
	    }
	    case 'v': {
		printf("%s %s\n", argv[0], VERSION);
		return 0;
	    }
	    case 'j': {
		if (!progs_dev_check(journal_dev = optarg)) {
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
	    case '?': {
		fsck_print_usage();
		return 0xfe;
	    }
	}
    }
	
    if (optind == argc) {
	fsck_print_usage();
	return 0xfe;
    }

    host_dev = argv[optind++];
	
    if (!progs_dev_check(host_dev)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Device %s doesn't exists or invalid."), host_dev);
	return 0xfe;	
    }
	
    if (!(host_dal = file_open(host_dev, DEFAULT_BLOCK_SIZE, 
	O_RDONLY)))
    {	    
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't open device device %s."), host_dev);
	return 0xfe;
    }	
	
    if (journal_dev) {
	if (!strcmp(journal_dev, host_dev))
	    journal_dal = NULL;
	else {
	    if (!(journal_dal = file_open(journal_dev, dal_get_blocksize(host_dal), 
		O_RDONLY)))
	    {	    
		libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		    _("Couldn't open device %s."), journal_dev);
		return 0xfe;
	    }	
	}    
    }
	
    if (!(fs = reiserfs_fs_open(host_dal, 
	    !journal ? NULL : (journal_dal ? journal_dal : host_dal))))
	goto error_free_journal_dal;
	
    libreiserfs_exception_throw(EXCEPTION_NO_FEATURE, EXCEPTION_IGNORE, 
	_("Sorry, not implemented yet!"));
	
    reiserfs_fs_close(fs);
	    
    if (journal_dal) {
    	dal_sync(journal_dal);
    	file_close(journal_dal);
    }
		
    dal_sync(host_dal);
    file_close(host_dal);    

    return 0;
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

