/*
    This code (with some modifications) is from GNU Parted
    Copyright (C) 1999, 2000, 2001 Free Software Foundation, Inc.
*/

#ifndef DEBUG_H
#define DEBUG_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef DEBUG

#ifdef __GNUC__

#define ASSERT(cond, action)		    \
    do {				    \
    	if (!libreiserfs_assert(cond,	    \
	    #cond,			    \
	     __FILE__,			    \
	     __LINE__,			    \
	     __PRETTY_FUNCTION__))	    \
	{				    \
	    action;			    \
	}				    \
    } while (0)

#else

#define ASSERT(cond, action)		    \
    do {				    \
	if (!libreiserfs_assert(cond,	    \
	     #cond,			    \
	     "unknown",			    \
	     0,				    \
	     "unknown"))		    \
	{				    \
	    action;			    \
	}				    \
    } while (0)

#endif

#else

#define ASSERT(cond, action) while (0) {}

#endif

extern int libreiserfs_assert(int cond, const char *cond_text, 
    const char *file, int line, const char *function);

#endif

