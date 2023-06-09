/*
    This code (with some modifications) is from GNU Parted
    Copyright (C) 1999, 2000, 2001 Free Software Foundation, Inc.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

static reiserfs_exception_option_t default_handler(reiserfs_exception_t *ex);
static reiserfs_exception_handler_t exception_handler = default_handler;
static reiserfs_exception_t *exception = NULL;

static int fetch_count = 0;
static int libreiserfs_exception = 0;

static char *type_strings[] = {
    N_("Information"),
    N_("Warning"),
    N_("Error"),
    N_("Fatal"),
    N_("Bug")
};

static char *option_strings[] = {
    N_("Yes"),
    N_("No"),
    N_("OK"),
    N_("Retry"),
    N_("Ignore"),
    N_("Cancel")
};

char *libreiserfs_exception_type_string(reiserfs_exception_type_t type) {
    return type_strings[type - 1];
}

reiserfs_exception_type_t libreiserfs_exception_type(reiserfs_exception_t *ex) {
    return ex->type;
}

char *libreiserfs_exception_option_string(reiserfs_exception_option_t opt) {
    return option_strings[reiserfs_tools_log2(opt)];
}

reiserfs_exception_option_t libreiserfs_exception_option(reiserfs_exception_t *ex) {
    return ex->options;
}

char *libreiserfs_exception_message(reiserfs_exception_t *ex) {
    return ex->message;
}

static reiserfs_exception_option_t default_handler(reiserfs_exception_t *ex) {
    if (ex->type == EXCEPTION_BUG)
	fprintf (stderr, _("A bug has been detected in libreiserfs. "
	    "Please email a bug report to torque@ukrpost.net containing the version (%s) "
	    "and the following message: "), VERSION);
    else
        fprintf (stderr, "%s: ", libreiserfs_exception_type_string(ex->type));

    fprintf (stderr, "%s\n", ex->message);

    switch (ex->options) {
        case EXCEPTION_OK:
        case EXCEPTION_CANCEL:
        case EXCEPTION_IGNORE:
            return ex->options;
	    
        default:
            return EXCEPTION_UNHANDLED;
    }
}

void libreiserfs_exception_set_handler(reiserfs_exception_handler_t handler) {
    if (handler)
	exception_handler = handler;
    else
	exception_handler = default_handler;
}

void libreiserfs_exception_catch(void) {
    if (libreiserfs_exception) {
	libreiserfs_exception = 0;
	libreiserfs_free(exception->message);
	libreiserfs_free(exception);
	exception = NULL;
    }
}

static reiserfs_exception_option_t do_throw(void) {
    reiserfs_exception_option_t opt;

    libreiserfs_exception = 1;

    if (fetch_count)
	return EXCEPTION_UNHANDLED;
    else {
	opt = exception_handler(exception);
	libreiserfs_exception_catch();
	return opt;
    }
}

reiserfs_exception_option_t libreiserfs_exception_throw(reiserfs_exception_type_t type,
    reiserfs_exception_option_t opts, const char* message, ...)
{
    va_list arg_list;
    reiserfs_gauge_t *libreiserfs_gauge;

    if (exception)
	libreiserfs_exception_catch();

    if (!(exception = (reiserfs_exception_t *)libreiserfs_calloc(sizeof(reiserfs_exception_t), 0)))
	goto no_memory;

    if (!(exception->message = (char *)libreiserfs_calloc(4096, 0)))
	goto no_memory;

    exception->type = type;
    exception->options = opts;

    va_start(arg_list, message);

#ifdef DJGPP
    vsprintf(exception->message, message, arg_list);
#else
    vsnprintf(exception->message, 4096, message, arg_list);
#endif
    
    va_end(arg_list);

    if ((libreiserfs_gauge = libreiserfs_get_gauge())) {
    
	if (libreiserfs_gauge->state != REISERFS_GAUGE_DONE && 
		libreiserfs_gauge->state != REISERFS_GAUGE_FAILED)
	    libreiserfs_gauge_finish(libreiserfs_gauge, 0);
    }
    
    return do_throw();

no_memory:
    fprintf(stderr, "Out of memory in exception handler!\n");

    va_start(arg_list, message);
    vprintf(message, arg_list);
    va_end(arg_list);

    return EXCEPTION_UNHANDLED;
}

reiserfs_exception_option_t libreiserfs_exception_rethrow(void) {
    return do_throw();
}

void libreiserfs_exception_fetch_all(void) {
    fetch_count++;
}

void libreiserfs_exception_leave_all(void) {
    ASSERT(fetch_count > 0, return);
    fetch_count--;
}

