/*
    dal.h -- device independent interface.
    Copyright (C) 2001, 2002 Yury Umanets.
*/

#ifndef DAL_H
#define DAL_H

#include <sys/types.h>

typedef unsigned long blk_t;
typedef unsigned long count_t;

struct dal_ops;

struct dal {
    int flags;
    void *data;
    void *entity;
    unsigned blocksize;
    struct dal_ops *ops;
    char name[256], error[256];
};

typedef struct dal dal_t;

struct dal_ops {
    int (*read)(dal_t *, void *, blk_t, count_t);
    int (*write)(dal_t *, void *, blk_t, count_t);
    int (*sync)(dal_t *);
    int (*flags)(dal_t *);
    int (*equals)(dal_t *, dal_t *);
    unsigned int (*stat)(dal_t *);
    count_t (*len)(dal_t *);
};

extern void *libdal_malloc(size_t size);
extern void *libdal_calloc(size_t size, char c);
extern int libdal_realloc(void **old, size_t size);
extern void libdal_free(void* ptr);

extern dal_t *dal_open(struct dal_ops *ops, unsigned blocksize, 
    int flags, void *data);

extern void dal_close(dal_t *dal);

extern int dal_set_blocksize(dal_t *dal, unsigned blocksize);
extern unsigned dal_get_blocksize(dal_t *dal);

extern int dal_read(dal_t *dal, void *buff, blk_t block, count_t count);
extern int dal_write(dal_t *dal, void *buff, blk_t block, count_t count);
extern int dal_sync(dal_t *dal);
extern int dal_flags(dal_t *dal);
extern int dal_equals(dal_t *dal1, dal_t *dal2);

extern unsigned int dal_stat(dal_t *dal);
extern count_t dal_len(dal_t *dal);

extern char *dal_name(dal_t *dal);
extern char *dal_error(dal_t *dal);

#endif
