/*
    file.h -- standard file device that works via device interface.
    Copyright (C) 2001, 2002 Yury Umanets.
*/

#ifndef FILE_DAL_H
#define FILE_DAL_H

#include <dal/dal.h>

extern dal_t *file_open(const char *file, unsigned blocksize, int flags);
extern int file_reopen(dal_t *dal, int flags);
extern void file_close(dal_t *dal);

#endif

