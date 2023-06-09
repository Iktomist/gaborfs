/*
    file.c -- standard file dal abstraction.
    Copyright (C) 2001, 2002 Yury Umanets.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#if defined(__freebsd__)
#  define O_LARGEFILE 0
#endif

#ifndef DJGPP
#  include <sys/stat.h>
#endif

#include <dal/dal.h>

static void file_save_error(dal_t *dal) {
    char *error;

    memset(dal->error, 0, sizeof(dal->error));
    
    if ((error = strerror(errno)))
	memcpy(dal->error, error, strlen(error));
}

static int file_read(dal_t *dal, void *buff, blk_t block, count_t count) {
    off_t off, len;
	
    if (!dal || !buff)
    	return 0;
	
    off = (off_t)block * (off_t)dal->blocksize;
    if (lseek(*((int *)dal->entity), off, SEEK_SET) == (off_t)-1) {
	file_save_error(dal);
	return 0;
    }

    len = (off_t)(count * dal->blocksize);
    if (read(*((int *)dal->entity), buff, len) <= 0) {
	file_save_error(dal);
	return 0;
    }
    
    return 1;
}

static int file_write(dal_t *dal, void *buff, blk_t block, count_t count) {
    off_t off, len;
	
    if (!dal || !buff)
	return 0;
	
    off = (off_t)block * (off_t)dal->blocksize;
    
    if (lseek(*((int *)dal->entity), off, SEEK_SET) == (off_t)-1) {
	file_save_error(dal);
	return 0;
    }
    
    len = (off_t)count * (off_t)dal->blocksize;
    if (write((*(int *)dal->entity), buff, len) <= 0) {
	file_save_error(dal);
	return 0;
    }
    
    return 1;
}

static int file_sync(dal_t *dal) {

    if (!dal) 
	return 0;
	
    if (fsync(*((int *)dal->entity))) {
	file_save_error(dal);
	return 0;
    }
    
    return 1;
}

static int file_flags(dal_t *dal) {

    if (!dal) 
	return 0;
		
    return dal->flags;
}

static int file_equals(dal_t *dal1, dal_t *dal2) {

    if (!dal1 || !dal2)
	return 0;
	  
    return !strcmp((char *)dal1->data, (char *)dal2->data);
}

static unsigned int file_stat(dal_t *dal) {
#ifdef DJGPP
    if (!dal) 
	return 0;
    
    return 1;
#else
    struct stat st;
	
    if (!dal)
	return 0;
	
    if (stat((char *)dal->data, &st))
	return 0;

    return (unsigned int)st.st_rdev;
#endif
}

#if defined(__linux__) && defined(_IOR) && !defined(BLKGETSIZE64)
#   define BLKGETSIZE64 _IOR(0x12, 114, sizeof(unsigned long long))
#endif

/*
    Handler for "len" operation for use with file device. See bellow for 
    understanding where it is used.
*/
static count_t file_len(dal_t *dal) {
    unsigned long long size;
    off_t max_off = 0;

    if (!dal) return 0;
    
#ifdef BLKGETSIZE64
    
    if (ioctl(*((int *)dal->entity), BLKGETSIZE64, &size) >= 0)
        return (count_t)(size / dal->blocksize);
    
    file_save_error(dal);
    
#endif

#ifdef BLKGETSIZE    
    
    if (ioctl(*((int *)dal->entity), BLKGETSIZE, &size) >= 0)
        return (count_t)(size / (dal->blocksize / 512));

    file_save_error(dal);
    
#endif
    
    if ((max_off = lseek(*((int *)dal->entity), 0, SEEK_END)) == (off_t)-1) {
	file_save_error(dal);
	return 0;
    }
    
    return (count_t)(max_off / dal->blocksize);
}

static struct dal_ops ops = {
    .read = file_read, 
    .write = file_write, 
    .sync = file_sync, 
    .flags = file_flags, 
    .equals = file_equals, 
    .stat = file_stat, 
    .len = file_len
};

dal_t *file_open(const char *file, unsigned blocksize, int flags) {
    int fd;
    dal_t *dal;
	
    if (!file) 
	return NULL;

#if defined(O_LARGEFILE)
    if ((fd = open(file, flags | O_LARGEFILE)) == -1)
#else
    if ((fd = open(file, flags)) == -1)
#endif
	return NULL;
	
    dal = dal_open(&ops, blocksize, flags, (void *)file);
    strncpy(dal->name, file, strlen(file));

    if (!(dal->entity = libdal_calloc(sizeof(int), 0)))
	goto error_free_dal;

    *((int *)dal->entity) = fd;
    
    return dal;
    
error_free_dal:
    dal_close(dal);
error:
    return NULL;    
}

int file_reopen(dal_t *dal, int flags) {
    int fd;
	
    if (!dal) 
	return 0;

    close(*((int *)dal->entity));
	
#if defined(O_LARGEFILE)
    if ((fd = open((char *)dal->data, flags | O_LARGEFILE)) == -1)
#else
    if ((fd = open((char *)dal->data, flags)) == -1)
#endif
	return 0;
	
    *((int *)dal->entity) = fd;
    dal->flags = flags;
	
    return 1;
}

void file_close(dal_t *dal) {

    if (!dal) 
	return;

    close(*((int *)dal->entity));
    libdal_free(dal->entity);
    dal_close(dal);
}

