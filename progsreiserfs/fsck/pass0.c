/*
    pass0.c -- the gathering some information needed by another stages of fsck.
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <dal/dal.h>
#include <reiserfs/reiserfs.h>

long gathering_leaves(reiserfs_block_t *node, void *data) {
    return 1;	
}

