/*
    callback.c -- reiserfs callback functions
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiserfs/reiserfs.h>

int reiserfs_callback_segment_gauge(reiserfs_segment_t *segment, reiserfs_block_t *block, 
    long number, reiserfs_gauge_t *gauge) 
{
	(void)block;

    if (gauge) {
	libreiserfs_gauge_set_value(gauge, (unsigned int)((number * 100) / 
	    reiserfs_segment_len(segment)));
    }	

    return 1;
}

