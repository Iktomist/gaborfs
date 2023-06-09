/*
    reiserfs.h -- reiserfs root include
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifndef REISERFS_H
#define REISERFS_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(__sparc__) || defined(__sparcv9)
#  include <sys/int_types.h>
#elif defined(__freebsd__)
#  include <inttypes.h>
#else
#  include <stdint.h>
#endif

#include "block.h"
#include "bitmap.h"
#include "journal.h"
#include "key.h"
#include "endian.h"
#include "filesystem.h"
#include "tree.h"
#include "tools.h"
#include "exception.h"
#include "segment.h"
#include "object.h"
#include "gauge.h"
#include "path.h"
#include "dir.h"
#include "file.h"
#include "core.h"
#include "node.h"

typedef void *(*libreiserfs_malloc_handler_t) (size_t);
typedef void *(*libreiserfs_realloc_handler_t) (void *, size_t);
typedef void (*libreiserfs_free_handler_t) (void *);

extern int libreiserfs_get_max_interface_version(void);
extern int libreiserfs_get_min_interface_version(void);
extern const char *libreiserfs_get_version(void);

extern void libreiserfs_set_gauge(reiserfs_gauge_t *gauge);
extern reiserfs_gauge_t *libreiserfs_get_gauge(void);

extern void libreiserfs_malloc_set_handler(libreiserfs_malloc_handler_t handler);
extern libreiserfs_malloc_handler_t libreiserfs_malloc_get_handler(void);

extern void *libreiserfs_malloc(size_t size);
extern void *libreiserfs_calloc(size_t size, char c);

extern void libreiserfs_realloc_set_handler(libreiserfs_realloc_handler_t handler);
extern libreiserfs_realloc_handler_t libreiserfs_realloc_get_handler(void);

extern int libreiserfs_realloc(void** old, size_t size);

extern void libreiserfs_free_set_handler(libreiserfs_free_handler_t handler);
extern libreiserfs_free_handler_t libreiserfs_free_get_handler(void);

extern void libreiserfs_free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif

