/*
	This code is from the original reiserfs code, as found in reiserfsprogs 
	and the linux kernel.
	Copyright (C) 1996-2002 Hans Reiser, see COPYING.NAMESYS for licensing
	and copyright details.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiserfs/hashes.h>

#define DELTA 		0x9E3779B9
#define FULLROUNDS 	10
#define PARTROUNDS 	6

#define __tea_core(rounds)							\
    do {									\
	    uint32_t sum = 0;							\
	    int n = rounds;							\
	    uint32_t b0, b1;							\
										\
	    b0 = h0;								\
	    b1 = h1;								\
										\
	    do {								\
			sum += DELTA;						\
			b0 += ((b1 << 4)+a) ^ (b1+sum) ^ ((b1 >> 5) + b);	\
			b1 += ((b0 << 4)+c) ^ (b0+sum) ^ ((b0 >> 5) + d);	\
	    } while(--n);							\
										\
	    h0 += b0;								\
	    h1 += b1;								\
	} while(0)

uint32_t __tea_hash_func(const signed char *name, int len) {
    uint32_t k[] = { 0x9464a485, 0x542e1a94, 0x3e846bff, 0xb75bcfc3}; 

    uint32_t h0 = k[0], h1 = k[1];
    uint32_t a, b, c, d;
    uint32_t pad;
    int i;
 

    pad = (uint32_t)len | ((uint32_t)len << 8);
    pad |= pad << 16;

    while(len >= 16) {
	a = (uint32_t)name[ 0]      |
	    (uint32_t)name[ 1] << 8 |
	    (uint32_t)name[ 2] << 16|
	    (uint32_t)name[ 3] << 24;
	b = (uint32_t)name[ 4]      |
	    (uint32_t)name[ 5] << 8 |
	    (uint32_t)name[ 6] << 16|
	    (uint32_t)name[ 7] << 24;
	c = (uint32_t)name[ 8]      |
	    (uint32_t)name[ 9] << 8 |
	    (uint32_t)name[10] << 16|
	    (uint32_t)name[11] << 24;
	d = (uint32_t)name[12]      |
	    (uint32_t)name[13] << 8 |
	    (uint32_t)name[14] << 16|
	    (uint32_t)name[15] << 24;
	
	__tea_core(PARTROUNDS);
	    
	len -= 16;
	name += 16;
    }

    if (len >= 12) {
        if (len >= 16)
            *(int *)0 = 0;

	    a = (uint32_t)name[ 0]      |
	        (uint32_t)name[ 1] << 8 |
	        (uint32_t)name[ 2] << 16|
	        (uint32_t)name[ 3] << 24;
	    b = (uint32_t)name[ 4]      |
	        (uint32_t)name[ 5] << 8 |
	        (uint32_t)name[ 6] << 16|
	        (uint32_t)name[ 7] << 24;
	    c = (uint32_t)name[ 8]      |
	        (uint32_t)name[ 9] << 8 |
		(uint32_t)name[10] << 16|
		(uint32_t)name[11] << 24;

	    d = pad;
	    for(i = 12; i < len; i++) {
	    	d <<= 8;
	    	d |= name[i];
	    }
    } else if (len >= 8) {
        if (len >= 12)
            *(int *)0 = 0;
        a = (uint32_t)name[ 0]      |
	    (uint32_t)name[ 1] << 8 |
	    (uint32_t)name[ 2] << 16|
	    (uint32_t)name[ 3] << 24;
	b = (uint32_t)name[ 4]      |
	    (uint32_t)name[ 5] << 8 |
	    (uint32_t)name[ 6] << 16|
	    (uint32_t)name[ 7] << 24;
	c = d = pad;
	    
	for (i = 8; i < len; i++) {
	    c <<= 8;
	    c |= name[i];
	}
    } else if (len >= 4) {
        if (len >= 8)
	    *(int *)0 = 0;
	a = (uint32_t)name[ 0]      |
	    (uint32_t)name[ 1] << 8 |
	    (uint32_t)name[ 2] << 16|
	    (uint32_t)name[ 3] << 24;
	b = c = d = pad;
	for (i = 4; i < len; i++) {
	    b <<= 8;
	    b |= name[i];
	}
    } else {
	if (len >= 4)
	    *(int *)0 = 0;
	a = b = c = d = pad;
	for(i = 0; i < len; i++) {
	    a <<= 8;
	    a |= name[i];
	}
    }

    __tea_core(FULLROUNDS);

    return h0 ^ h1;
}


uint32_t __yura_hash_func(const signed char *name, int len) {
    int j, pow;
    uint32_t a, c;
    int i;
	
    for (pow = 1, i = 1; i < len; i++) pow = pow * 10; 
	
    if (len == 1) 
	a = name[0] - 48;
    else
	a = (name[0] - 48) * pow;
	
    for (i = 1; i < len; i++) {
	c = name[i] - 48; 
	for (pow = 1, j = i; j < len - 1; j++) pow = pow * 10; 
	a = a + c * pow;
    }
	
    for (; i < 40; i++) {
	c = '0' - 48; 
	for (pow = 1,j = i; j < len - 1; j++) pow = pow * 10; 
	a = a + c * pow;
    }
	
    for (; i < 256; i++) {
	c = i; 
	for (pow = 1, j = i; j < len - 1; j++) pow = pow * 10; 
	a = a + c * pow;
    }
	
    a = a << 7;
    return a;
}


uint32_t __r5_hash_func(const signed char *name, int len) {
    uint32_t a = 0;
    int i;
	
    for (i = 0; i < len; i ++) {
	a += name[i] << 4;
	a += name[i] >> 4;
	a *= 11;
    } 
    return a;
}

