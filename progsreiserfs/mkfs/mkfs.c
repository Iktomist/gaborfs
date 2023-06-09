/*
    mkfs.c -- reiserfs filesysetm create program.
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
#include <sys/utsname.h>

#include <reiserfs/reiserfs.h>
#include <reiserfs/libprogs_tools.h>

#include <dal/file.h>

#ifdef HAVE_UUID		
#  include <uuid/uuid.h>
#endif

#if ENABLE_NLS
#  include <locale.h>
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif

static void mkfs_print_usage(void) {
    fprintf(stderr, _("Usage: mkfs.reiserfs [ options ] device [ size[K|M|G] ]\n"));
    fprintf(stderr, _("Options:\n"
	"  -v | --version                  prints current version\n"
	"  -u | --usage                    prints program usage\n"
	"  -s N | --journal-size=N         journal size\n"
	"  -o N | --journal-offset=N       journal offset for relocated journal\n"));
    fprintf(stderr, _(
	"  -t N | --transaction-max-size=N maximal transaction size\n"
	"  -b N | --block-size=N           block size (1024, 2048, 4096...)\n"
	"  -f FORMAT | --format=FORMAT     reiserfs version (3.5 or 3.6)\n"
	"  -h HASH | --hash=HASH           hash function (tea, yura or r5)\n"
	"  -j FILE | --journal-device=FILE journal device for separated journal\n"));
    fprintf(stderr, _(
	"  -l LABEL | --label=LABEL        volume label\n"
	"  -d UUID | --uuid=UUID           sets universally unique identifier\n"
	"  -q | --quiet                    non-interactive mode\n"));
}

#if defined (__linux__)
static char *mkfs_kernel_version() {
    struct utsname info;
    char *kernel;
	
    if (uname(&info) == -1)
	return NULL;
	
    if (!(kernel = libreiserfs_malloc(strlen(info.release) + 1)))
	return NULL;
    memset(kernel, 0, strlen(info.release) + 1);
	
    strncpy(kernel, info.release, strlen(info.release));
    return kernel;
}
#endif

static int mkfs_recomended_format() {
    int format = FS_FORMAT_3_6;
	
#if defined(__linux__)
    char *kernel;
	
    if (!(kernel = mkfs_kernel_version()))
	return FS_FORMAT_3_5;
	
    if (!strncmp(kernel, "2.4", 3) || !strncmp(kernel, "2.5", 3))
	format = FS_FORMAT_3_6;
	
    if (!strncmp(kernel, "2.2", 3))
	format = FS_FORMAT_3_5;
	
    libreiserfs_free(kernel);
#endif

    return format;
}

static int mkfs_format(const char *format_str) {
    const char *recomended_format_str;
    int format, recomended_format = mkfs_recomended_format();
	
    if (!format_str || strlen(format_str) == 0)
	return recomended_format;
	
    if ((format = reiserfs_fs_format_from_str(format_str)) == -1)
	return -1;
	
#if defined(__linux__)
    if (format != recomended_format) {
	char *kernel;
	
	if (!(kernel = mkfs_kernel_version()))
	    return -1;
    
	recomended_format_str = reiserfs_fs_short_format_str(recomended_format);
	libreiserfs_exception_throw(EXCEPTION_WARNING, EXCEPTION_IGNORE, 
	    _("For kernel %s recomended version of reiserfs is %s, but selected %s."), 
	    kernel, recomended_format_str, format_str);
	
	libreiserfs_free(kernel);
    }
#endif
	
    return format;
}

int main(int argc, char *argv[]) {
    reiserfs_fs_t *fs;
    char *host_dev = NULL, *journal_dev = NULL;
    dal_t *host_dal, *journal_dal = NULL;
    reiserfs_gauge_t *gauge = NULL;
    reiserfs_hash_t hash = R5_HASH;
	
    int quiet = 0, choice = 0, error = 0;
    int blocksize = DEFAULT_BLOCK_SIZE;
    int format = mkfs_recomended_format();
	
    long start = 0, len = 0;
    long fs_len = 0, dev_len;
    long max_trans_size = 0;
	
    char uuid[17];
    char *label = NULL;

    char mess_part[4096];
	
    static struct option long_options[] = {
	{"version", no_argument, NULL, 'v'},
	{"usage", no_argument, NULL, 'u'},
	{"block-size", required_argument, NULL, 'b'},
	{"format", required_argument, NULL, 'f'},
	{"hash", required_argument, NULL, 'h'},
	{"journal-device", required_argument, NULL, 'j'},
	{"journal-size", required_argument, NULL, 's'},
	{"journal-offset", required_argument, NULL, 'o'},
	{"transaction-max-size", required_argument, NULL, 't'},
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
	
    if (argc < 2) {
	mkfs_print_usage();
	return 0xfe;
    }
	
    memset(uuid, 0, sizeof(uuid));

    while ((choice = getopt_long_only(argc, argv, "uvb:f:h:j:s:o:t:i:l:q", long_options, 
	(int *)0)) != EOF) 
    {
	switch (choice) {
	    case 'u': {
		mkfs_print_usage();
		return 0;
	    }
	    case 'v': {
		printf("%s %s\n", argv[0], VERSION);
		return 0;
	    }
	    case 'b': {
	        if (!(blocksize = progs_strtol(optarg, &error)) && error) {
		    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		        _("Invalid blocksize (%s)."), optarg);
		    return 0xfe;
		}
		if (blocksize < 1024) {
		    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
			_("Invalid blocksize (%s). Must be 1024, 2048..."), optarg);
		    return 0xfe;
		}
		if (!reiserfs_tools_power_of_two(blocksize)) {
		    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
			_("Invalid block size. It must power of two."));
		    return 0xfe;	
		}
		break;
	    }
	    case 'h': {
		if (!(hash = reiserfs_fs_hash_from_str(optarg))) {
		    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
			_("Invalid hash function (%s)."), optarg);
		    return 0xfe;	
		}
		break;
	    }
	    case 'f': {
		if ((format = mkfs_format(optarg)) == -1) {
		    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
			_("Invalid filesystem format. Use 3.5 or 3.6 please."));
		    return 0xfe;
		}
		break;
	    }
	    case 'j': {
		journal_dev = optarg;
		if (!progs_dev_check(journal_dev)) {
		    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
			_("Device %s doesn't exists or invalid."), journal_dev);
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
		if (!(start  = progs_strtol(optarg, &error)) && error) {
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
#ifdef HAVE_UUID		
		if (uuid_is_null(optarg) || strlen(optarg) < 16) {
#else
		if (strlen(optarg) < 16) {
#endif
		    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
			_("Invalid uuid (%s)."), optarg);
		    return 0xfe;
		}
		strncpy(uuid, optarg, sizeof(uuid));
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
	        mkfs_print_usage();
	        return 0xfe;
	    }
	}
    }

    if (optind >= argc) {
	mkfs_print_usage();
	return 0xfe;
    }
	
    host_dev = argv[optind++];
	
    if (optind < argc) {
	char *fs_len_str = argv[optind];
		
	libreiserfs_exception_fetch_all();
	if (!progs_dev_check(host_dev)) {
	    char *tmp = host_dev;
			
	    host_dev = fs_len_str;
	    fs_len_str = tmp;
	}
	libreiserfs_exception_leave_all();
		
	if (!(fs_len = progs_digit_parse(fs_len_str, blocksize, &error)) && error) {
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL,
		_("Invalid filesystem size (%s)."), fs_len_str);
	    return 0xfe;	
	}
    }
	
    /* Checking given device for validness */
    if (!progs_dev_check(host_dev)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Device %s doesn't exists or invalid."), host_dev);
	return 0xfe;
    }
	
    /* Creating device abstraction layer */
    if (!(host_dal = file_open(host_dev, blocksize, O_RDWR))) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Couldn't open device %s. %s."), host_dev, strerror(errno));
	goto error;
    }
	
    memset(mess_part, 0, sizeof(mess_part));
    if (journal_dev) {
        if (!strcmp(journal_dev, host_dev))
	    journal_dal = NULL;
	else {
	    if (!(journal_dal = file_open(journal_dev, blocksize, O_RDWR))) {
		libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		    _("Couldn't open device %s. %s."), journal_dev, strerror(errno));
		goto error_free_host_dal;
	    }
	    sprintf(mess_part, _(" and %s"), journal_dev);
	}    
    }
	
    dev_len = (long)dal_len(host_dal);
	
    if (!fs_len) 
        fs_len = dev_len;
	
    if (fs_len > dev_len) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Filesystem size is too big for device (%lu)."), dev_len);
	goto error_free_host_dal;
    }	
	
    if (journal_dal == NULL && start > 0) {
	libreiserfs_exception_throw(EXCEPTION_WARNING, EXCEPTION_IGNORE, 
	    _("Filesystem with journal on host device has been selected. "
	    "Parameter journal-offset will be ignored."));
	start = 0;    
    }    

    if (journal_dal != NULL) {
        dev_len = dal_len(journal_dal);

        if (!len)
	    len = dal_len(journal_dal) - start - 1;

	if (start + len + 1 > (long)dev_len) {
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
		_("Can't allocate journal (%lu - %lu) outside the device (%lu)."), 
		start, len + 1, dev_len);
	    goto error_free_journal_dal;
	}
    } else {
	if (!len) 
	    len = (blocksize == 1024 ? DEFAULT_JOURNAL_SIZE_BS1024 : 
		DEFAULT_JOURNAL_SIZE_BS4096);
    }

    if (!max_trans_size) {
	max_trans_size = JOURNAL_MAX_TRANS;
	if (blocksize < 4096)
	    max_trans_size = JOURNAL_MAX_TRANS / (4096 / blocksize);
    }

#ifdef HAVE_UUID		
    if ((label || !uuid_is_null(uuid)) && format == FS_FORMAT_3_5) {
#else
    if (label && format == FS_FORMAT_3_5) {
#endif    
        libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	    _("Sorry, reiserfs 3.5 doesn't supports uuid and label."));
	goto error_free_journal_dal;
    }
	
#ifdef HAVE_UUID		
    if (!strlen(uuid))
        uuid_generate(uuid);
#endif

    choice = 'y';
    if (!quiet) {
        if (!(choice = progs_choose("ynYN", _("Please select (y/n) "), 
		_("All data on %s%s will be lost. Do you realy want to create %s "
		"(y/n) "), host_dev, mess_part, reiserfs_fs_long_format_str(format))))
	    goto error_free_journal_dal;
    }
	
    if (choice == 'n' || choice == 'N')
        goto error_free_journal_dal;

    fprintf(stderr, _("Creating %s with %s journal on %s%s\n"),
        reiserfs_fs_long_format_str(format), (journal_dal ? _("relocated") : 
        _("standard")), host_dev, mess_part);
	
    fflush(stderr);
    
    if (!(gauge = libreiserfs_gauge_create(REISERFS_GAUGE_PERCENTAGE, NULL, NULL)))
	goto error_free_journal_dal;
	
    libreiserfs_set_gauge(gauge);

    if (!(fs = reiserfs_fs_create(host_dal, (journal_dal ? journal_dal : host_dal), 
        start, max_trans_size, len, blocksize, format, hash, label, uuid, fs_len))) 
    {
        libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, 
	   _("Couldn't create filesystem on %s."), host_dev);
	goto error_free_gauge;
    }

    libreiserfs_gauge_free(gauge);
    libreiserfs_set_gauge(NULL);
    
    reiserfs_fs_close(fs);

    if (!(gauge = libreiserfs_gauge_create(REISERFS_GAUGE_SILENT, _("syncing"), NULL)))
	goto error_free_journal_dal;

    libreiserfs_set_gauge(gauge);
	    
    if (journal_dal) {
        dal_sync(journal_dal);
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
	
error_free_fs:
    reiserfs_fs_close(fs);
error_free_gauge:
    libreiserfs_gauge_free(gauge);
    libreiserfs_set_gauge(NULL);
error_free_journal_dal:
    if (journal_dal) 
        file_close(journal_dal);
error_free_host_dal:
    file_close(host_dal);
error:
    return 0xff;
}

