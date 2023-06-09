/*
    tools.c -- reiserfs tools
    Copyright (C) 2001, 2002 Yury Umanets <torque@ukrpost.net>, see COPYING for 
    licensing and copyright details.
	
    Some parts of this code stolen somewhere from linux.
*/

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

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <reiserfs/endian.h>
#include <reiserfs/tools.h>

static inline int reiserfs_tools_le_set_bit(int nr, void *addr) {
    uint8_t * p, mask;
    int retval;

    p = (uint8_t *)addr;
    p += nr >> 3;
    mask = 1 << (nr & 0x7);

    retval = (mask & *p) != 0;
    *p |= mask;

    return retval;
}

static inline int reiserfs_tools_le_clear_bit(int nr, void *addr) {
    uint8_t * p, mask;
    int retval;

    p = (uint8_t *)addr;
    p += nr >> 3;
    mask = 1 << (nr & 0x7);

    retval = (mask & *p) != 0;
    *p &= ~mask;
    
    return retval;
}

static inline int reiserfs_tools_le_test_bit(int nr, const void *addr) {
    uint8_t *p, mask;
  
    p = (uint8_t *)addr;
    p += nr >> 3;
    mask = 1 << (nr & 0x7);
    return ((mask & *p) != 0);
}

static inline int reiserfs_tools_le_find_first_zero_bit(const void *vaddr, unsigned size) {
    const uint8_t *p = vaddr, *addr = vaddr;
    int res;

    if (!size)
	return 0;

    size = (size >> 3) + ((size & 0x7) > 0);
    while (*p++ == 255) {
	if (--size == 0)
	return (p - addr) << 3;
    }
  
    --p;
    for (res = 0; res < 8; res++)
	if (!reiserfs_tools_test_bit (res, p)) break;
    return (p - addr) * 8 + res;
}

static inline int reiserfs_tools_le_find_next_zero_bit(const void *vaddr, 
    unsigned size, unsigned offset) 
{
    const uint8_t *addr = vaddr;
    const uint8_t *p = addr + (offset >> 3);
    int bit = offset & 7, res;
  
    if (offset >= size)
	return size;
  
    if (bit) {
	/* Look for zero in first char */
	for (res = bit; res < 8; res++)
	    if (!reiserfs_tools_test_bit (res, p))
		return (p - addr) * 8 + res;
	    p++;
    }

    /* No zero yet, search remaining full bytes for a zero */
    res = reiserfs_tools_find_first_zero_bit (p, size - 8 * (p - addr));
    return (p - addr) * 8 + res;
}

static inline int reiserfs_tools_be_set_bit(int nr, void *addr) {
    uint8_t mask = 1 << (nr & 0x7);
    uint8_t *p = (uint8_t *) addr + (nr >> 3);
    uint8_t old = *p;

    *p |= mask;
    return (old & mask) != 0;
}
 
static inline int reiserfs_tools_be_clear_bit(int nr, void *addr) {
    uint8_t mask = 1 << (nr & 0x07);
    uint8_t *p = (unsigned char *) addr + (nr >> 3);
    uint8_t old = *p;

    *p = *p & ~mask;
    return (old & mask) != 0;
}
 
static inline int reiserfs_tools_be_test_bit(int nr, const void *addr) {
    const uint8_t *ADDR = (__const__ uint8_t *) addr;
    return ((ADDR[nr >> 3] >> (nr & 0x7)) & 1) != 0;
}
 
static inline int reiserfs_tools_be_find_first_zero_bit(const void *vaddr, unsigned size) {
    return reiserfs_tools_find_next_zero_bit(vaddr, size, 0);
}

static inline unsigned long reiserfs_tools_ffz(unsigned long word) {
    unsigned long result = 0;
 
    while(word & 1) {
	result++;
	word >>= 1;
    }
    return result;
}

static inline int reiserfs_tools_be_find_next_zero_bit(const void *vaddr, unsigned size, 
    unsigned offset) 
{
    uint32_t *p = ((uint32_t *) vaddr) + (offset >> 5);
    uint32_t result = offset & ~31UL;
    uint32_t tmp;

    if (offset >= size)
	return size;

    size -= result;
    offset &= 31UL;
    if (offset) {
	tmp = *(p++);
	tmp |= SWAP32(~0UL >> (32-offset));
	if (size < 32)
	    goto found_first;
	
	if (~tmp)
	    goto found_middle;
	
	size -= 32;
	result += 32;
    }
    while (size & ~31UL) {
        if (~(tmp = *(p++)))
            goto found_middle;
        result += 32;
        size -= 32;
    }
    if (!size)
        return result;
    tmp = *p;

found_first:
    return result + reiserfs_tools_ffz(SWAP32(tmp) | (~0UL << size));
found_middle:
    return result + reiserfs_tools_ffz(SWAP32(tmp));
}

int reiserfs_tools_set_bit(int nr, void *addr) {
#ifndef WORDS_BIGENDIAN    
    return reiserfs_tools_le_set_bit(nr, addr);
#else 
    return reiserfs_tools_be_set_bit(nr, addr);
#endif 
    return 0;
}

int reiserfs_tools_clear_bit(int nr, void *addr) {
#ifndef WORDS_BIGENDIAN    
    return reiserfs_tools_le_clear_bit(nr, addr);
#else 
    return reiserfs_tools_be_clear_bit(nr, addr);
#endif 
    return 0;
}

int reiserfs_tools_test_bit(int nr, const void *addr) {
#ifndef WORDS_BIGENDIAN    
    return reiserfs_tools_le_test_bit(nr, addr);
#else 
    return reiserfs_tools_be_test_bit(nr, addr);
#endif 
    return 0;
}

int reiserfs_tools_find_first_zero_bit(const void *vaddr, unsigned size) {
#ifndef WORDS_BIGENDIAN    
    return reiserfs_tools_le_find_first_zero_bit(vaddr, size);
#else 
    return reiserfs_tools_be_find_first_zero_bit(vaddr, size);
#endif 
    return 0;
}

int reiserfs_tools_find_next_zero_bit(const void *vaddr, unsigned size, unsigned offset) {
#ifndef WORDS_BIGENDIAN    
    return reiserfs_tools_le_find_next_zero_bit(vaddr, size, offset);
#else 
    return reiserfs_tools_be_find_next_zero_bit(vaddr, size, offset);
#endif 
    return 0;
}
	
/* Signature checking functions */
int reiserfs_tools_3_5_signature(const char *signature) {
    return(!strncmp(signature, REISERFS_3_5_SUPER_SIGNATURE, 
	strlen(REISERFS_3_5_SUPER_SIGNATURE)));
}

int reiserfs_tools_3_6_signature(const char *signature) {
    return(!strncmp(signature, REISERFS_3_6_SUPER_SIGNATURE, 
	strlen(REISERFS_3_6_SUPER_SIGNATURE)));
}

int reiserfs_tools_journal_signature(const char *signature) {
    return(!strncmp(signature, REISERFS_JR_SUPER_SIGNATURE, 
    	strlen(REISERFS_JR_SUPER_SIGNATURE)));
}

int reiserfs_tools_any_signature(const char *signature) {
    if (reiserfs_tools_3_5_signature(signature) || 
	    reiserfs_tools_3_6_signature(signature) ||
	    reiserfs_tools_journal_signature(signature))
	return 1;
    return 0;
}

/* Other functions */
int reiserfs_tools_power_of_two(unsigned long value) {
    return (value & -value) == value;
}

int reiserfs_tools_log2(int n) {
    int x;
    for (x = 0; 1 << x <= n; x++);
	return x - 1;
}

uint32_t reiserfs_tools_random(void) {
    srandom(time(0));
    return random();
}

/* Search tools */
int reiserfs_tools_comp_generic(void *value1, void *value2) {
    uint32_t *val1 = (uint32_t *)value1, *val2 = (uint32_t *)value2;
	
    if (*val1 == *val2)
	return 0;
	
    if (*val1 < *val2)
	return -1;
    else	
	return 1;
}

int reiserfs_tools_fast_search(void *needle, void *array, int count,
	uint32_t width, reiserfs_comp_func_t comp_func, uint32_t *pos)
{
    int left, right, i, res;

    if (count == 0) {
    	*pos = 0;
        return 0;
    }

    left = 0;
    right = count - 1;

    for (i = (right + left) / 2; left <= right; i = (right + left) / 2) {
	switch (comp_func(array + (i * width), needle)) {
	   case -1: {
		left = i + 1;
		continue;
	    }
	    case 1: {
		if (i == 0) {
		    *pos = left;
		    return 0;
		}    
		right = i - 1;
		continue;
	    }
	    case 0: {
		*pos = i;
		return 1;
	    }	
	}
    }
	
    *pos = left;
    return 0;
}

