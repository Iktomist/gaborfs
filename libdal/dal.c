/*
    dal.c -- device abstraction layer.
    Copyright (C) 2001, 2002 Yury Umanets.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <dal/dal.h>

#define dal_check_param(param, action) \
    do { \
	if (!(param)) \
	    action; \
    } while (0)

#define dal_check_routine(dal, routine, action) \
    do { \
	if (!dal->ops->routine) \
	    action; \
    } while (0)

static int pow_of_two(unsigned long value) {
    return (value & -value) == value;
}

void *libdal_malloc(size_t size) {
    void *mem;

    mem = (void *)malloc(size);
    if (!mem) {
	fprintf(stderr, "Out of memory.\n");
	return NULL;
    }

    return mem;
}

void *libdal_calloc(size_t size, char c) {
    void *mem;

    if (!(mem = libdal_malloc(size)))
	return NULL;
    memset(mem, c, size);
    return mem;
}

int libdal_realloc(void **old, size_t size) {
    void *mem;

    mem = (void *)realloc(*old, size);
    if (!mem) {
	fprintf(stderr, "Out of memory.\n");
	return 0;
    }
    *old = mem;
    return 1;
}

void libdal_free(void* ptr) {
    free(ptr);
}

dal_t *dal_open(struct dal_ops *ops, unsigned blocksize, 
    int flags, void *data) 
{
    dal_t *dal;

    dal_check_param(ops, return NULL);
    
    if (!pow_of_two(blocksize))
	return NULL;
	
    if (!(dal = (dal_t *)libdal_malloc(sizeof(*dal))))
	return NULL;

    dal->ops = ops;
    dal->data = data;
    dal->flags = flags;
    dal->blocksize = blocksize;
	
    return dal;
}

void dal_close(dal_t *dal) {
    dal_check_param(dal, return);
    libdal_free(dal);
}

int dal_set_blocksize(dal_t *dal, unsigned blocksize) {

    dal_check_param(dal, return 0);
	
    if (!pow_of_two(blocksize))
	return 0;
    
    dal->blocksize = blocksize;
	
    return 1;
}

unsigned dal_get_blocksize(dal_t *dal) {
    dal_check_param(dal, return 0);
    return dal->blocksize;
}

int dal_read(dal_t *dal, void *buff, blk_t block, count_t count) {
    dal_check_param(dal, return 0);
    dal_check_routine(dal, read, return 0);
    return dal->ops->read(dal, buff, block, count);
}

int dal_write(dal_t *dal, void *buff, blk_t block, count_t count) {
    dal_check_param(dal, return 0);
    dal_check_param(buff, return 0);
    dal_check_routine(dal, write, return 0);
    return dal->ops->write(dal, buff, block, count);
}

int dal_sync(dal_t *dal) {
    dal_check_param(dal, return 0);
    dal_check_routine(dal, sync, return 0);
    return dal->ops->sync(dal);
}

int dal_flags(dal_t *dal) {
    dal_check_param(dal, return 0);
    dal_check_routine(dal, flags, return 0);
    return dal->ops->flags(dal);
}

int dal_equals(dal_t *dal1, dal_t *dal2) {
    dal_check_param(dal1, return 0);
    dal_check_param(dal2, return 0);
    dal_check_routine(dal1, equals, return 0);
    return dal1->ops->equals(dal1, dal2);
}

unsigned int dal_stat(dal_t *dal) {
    dal_check_param(dal, return 0);
    dal_check_routine(dal, stat, return 0);
    return dal->ops->stat(dal);
}

count_t dal_len(dal_t *dal) {
    dal_check_param(dal, return 0);
    dal_check_routine(dal, len, return 0);
    return dal->ops->len(dal);
}

char *dal_name(dal_t *dal) {
    dal_check_param(dal, return NULL);
    return dal->name;
}

char *dal_error(dal_t *dal) {
    return dal->error;
}

