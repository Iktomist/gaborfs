/*
    libprogs_tools.h -- some tools for progsreiserfs
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
*/

#ifndef LIBPROGS_TOOLS_H
#define LIBPROGS_TOOLS_H

extern long progs_strtol(const char *str, int *error);

extern int progs_dev_check(const char *dev);

extern int progs_choose(const char *chooses, const char *error, 
    const char *format, ...);

extern int progs_choose_check(const char *chooses, int choose);

extern int progs_digit_check(const char *str);
extern long progs_digit_parse(const char *str, size_t blocksize, int *error);

#endif
