/*
    tools.c -- common tools for the all progs.
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>

#include <reiserfs/exception.h>
#include <reiserfs/debug.h>
#include <reiserfs/libprogs_tools.h>

#if ENABLE_NLS
#  include <locale.h>
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif

#define KB 1024
#define MB (1024 * KB)
#define GB (1024 * MB)

long progs_strtol(const char *str, int *error) {
    char *err;
    long result = 0;

    if (error)
	*error = 0;
	
    if (!str) {
	if (error) *error = 1; 
	return 0;
    }	
	
    result = strtol(str, &err, 10);
	
    if (errno == ERANGE || *err) {
	if (error) *error = 1;
	return 0;
    }	
	
    return result;
}

/* Choose functions */
int progs_choose_check(const char *chooses, int choose) {
    unsigned i;
	
    if (!chooses) return 0;
	
    for (i = 0; i < strlen(chooses); i++)
	if (chooses[i] == choose) return 1;
	
    return 0;
}

int progs_choose(const char *chooses, const char *error, const char *format, ...) {
    va_list args;
    int choose, prompts = 0;
    char mess[4096], buf[255];
	
    if (!chooses || !format || !error)
	return 0;
	
    memset(mess, 0, 4096);
	
    va_start(args, format);
    vsprintf(mess, format, args);
    va_end(args);
	
    fprintf(stderr, mess);
    fflush(stderr);
	
    do {
	memset(buf, 0, 255);
		
	fgets(buf, 255, stdin);
	choose = buf[0];
			
	if (progs_choose_check(chooses, choose)) 
	    break;
		
	if (prompts < 2) {
	    fprintf(stderr, error);
	    fflush(stderr);
	}	
    } while (prompts++ < 2);
	
    if (!progs_choose_check(chooses, choose))
	choose = 0;
	
    return choose;
}

/* Device functions */
int progs_dev_check(const char *dev) {
    struct stat st;
	
    if (!dev)
	return 0;
	
    if (stat(dev, &st) == -1)
	return 0;
	
    if (!S_ISBLK(st.st_mode)) {
	libreiserfs_exception_throw(EXCEPTION_WARNING, EXCEPTION_IGNORE, 
	    _("Device %s isn't a block device."), dev);
    }
	
    return 1;
}

/* Parsing functions */
int progs_digit_check(const char *str) {
    int error = 0;

    progs_digit_parse(str, 4096, &error);
    return !error;
}

long progs_digit_parse(const char *str, size_t blocksize, int *error) {
    long long size;
    char number[255], label = 0;
	 
    if (error)
	*error = 0;
	
    if (!str || strlen(str) == 0 || !blocksize) {
	if (error) *error = 1;
	return 0;
    }	
	
    memset(number, 0, 255);
    strncpy(number, str, strlen(str));
    label = number[strlen(number) - 1];
	
    if (label == 'K' || label == 'M' || label == 'G')
	number[strlen(number) - 1] = '\0';
    else
	label = 0;	
	
    if ((size = progs_strtol(number, error)) == 0 && *error)
	return 0;
	
    if (label == 0 || label == 'M')
	size = size * MB;
    else if (label == 'K')
	size = size * KB;
    else if (label == 'G')
	size = size * GB;

    return size / blocksize;
}

