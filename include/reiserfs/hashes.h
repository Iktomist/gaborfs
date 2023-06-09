/*
    This code is from the original reiserfs code, as found in reiserfsprogs 
    and the linux kernel.
    Copyright (C) 1996-2002 Hans Reiser, see COPYING.NAMESYS for licensing
    and copyright details.
*/

#ifndef HASHES_H
#define HASHES_H

#if defined(__sparc__) || defined(__sparcv9)
#  include <sys/int_types.h>
#elif defined(__freebsd__)
#  include <inttypes.h>
#else
#  include <stdint.h>
#endif

#define GET_HASH_VALUE(offset) ((offset) & 0x7fffff80)

typedef uint32_t (*reiserfs_hash_func_t)(const signed char *, int);

extern uint32_t __tea_hash_func(const signed char *name, int len);
extern uint32_t __yura_hash_func(const signed char *name, int len);
extern uint32_t __r5_hash_func(const signed char *name, int len);

#endif

