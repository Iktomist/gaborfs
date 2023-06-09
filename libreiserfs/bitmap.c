/*
    bitmap.c -- bitmap functions.
    Copyright (C) 1996-2002 Hans Reiser.
    Author Yury Umanets.
*/

#include <stdlib.h>
#include <string.h>

#include <reiserfs/debug.h>
#include <reiserfs/reiserfs.h>

#define reiserfs_bitmap_range_check(bitmap, blk, action) \
    do { \
	if (blk >= bitmap->total_blocks) { \
	    libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_CANCEL, \
		"Block %lu is out of range (0-%lu)", blk, bitmap->total_blocks); \
	    action; \
	} \
    } while (0); \
	

void reiserfs_bitmap_use_block(reiserfs_bitmap_t *bitmap, blk_t blk) {
    ASSERT(bitmap != NULL, return);

    reiserfs_bitmap_range_check(bitmap, blk, return);
    if (reiserfs_tools_test_bit(blk, bitmap->map))
	return;
	
    reiserfs_tools_set_bit(blk, bitmap->map);
    bitmap->used_blocks++;
}

void reiserfs_bitmap_unuse_block(reiserfs_bitmap_t *bitmap, blk_t blk) {
    ASSERT(bitmap != NULL, return);

    reiserfs_bitmap_range_check(bitmap, blk, return);
    if (!reiserfs_tools_test_bit(blk, bitmap->map))
	return;
	
    reiserfs_tools_clear_bit(blk, bitmap->map);
    bitmap->used_blocks--;
}

int reiserfs_bitmap_test_block(reiserfs_bitmap_t *bitmap, blk_t blk) {
    ASSERT(bitmap != NULL, return 0);

    reiserfs_bitmap_range_check(bitmap, blk, return 0);
    return reiserfs_tools_test_bit(blk, bitmap->map);
}

blk_t reiserfs_bitmap_find_free(reiserfs_bitmap_t *bitmap, blk_t start) {
    blk_t blk;
	
    ASSERT(bitmap != NULL, return 0);
	
    reiserfs_bitmap_range_check(bitmap, start, return 0);
    if ((blk = reiserfs_tools_find_next_zero_bit(bitmap->map, 
	    bitmap->total_blocks, start)) >= bitmap->total_blocks)
	return 0;

    return blk;
}

static blk_t reiserfs_bitmap_calc(reiserfs_bitmap_t *bitmap, 
    blk_t start, blk_t end, int is_free) 
{
    blk_t i, blocks = 0;
	
    ASSERT(bitmap != NULL, return 0);
	
    reiserfs_bitmap_range_check(bitmap, start, return 0);
    reiserfs_bitmap_range_check(bitmap, end - 1, return 0);
	
    for (i = start; i < end; ) {
#if !defined(__sparc__) && !defined(__sparcv9)
	uint64_t *block64 = (uint64_t *)(bitmap->map + (i >> 0x3));
	uint16_t bits = sizeof(uint64_t) * 8;
		
	if (i + bits < end && i % 0x8 == 0 &&
	    *block64 == (is_free == 0 ? 0xffffffffffffffffLL : 0)) 
	{
	    blocks += bits;
	    i += bits;
	} else {
	    blocks += (reiserfs_bitmap_test_block(bitmap, i) ? is_free : !is_free);
	    i++;
	}
#else
	blocks += (reiserfs_bitmap_test_block(bitmap, i) ? is_free : !is_free);
	i++;	
#endif
    }
    return blocks;
}

blk_t reiserfs_bitmap_calc_used(reiserfs_bitmap_t *bitmap) {
    return reiserfs_bitmap_calc(bitmap, 0, bitmap->total_blocks, 0);
}

blk_t reiserfs_bitmap_calc_unused(reiserfs_bitmap_t *bitmap) {
    return reiserfs_bitmap_calc(bitmap, 0, bitmap->total_blocks, 1);
}

blk_t reiserfs_bitmap_calc_used_in_area(reiserfs_bitmap_t *bitmap, 
    blk_t start, blk_t end) 
{
    ASSERT(bitmap != NULL, return 0);
    return reiserfs_bitmap_calc(bitmap, start, end, 0);
}

blk_t reiserfs_bitmap_calc_unused_in_area(reiserfs_bitmap_t *bitmap, 
    blk_t start, blk_t end) 
{
    ASSERT(bitmap != NULL, return 0);
    return reiserfs_bitmap_calc(bitmap, start, end, 1);
}

blk_t reiserfs_bitmap_used(reiserfs_bitmap_t *bitmap) {
    ASSERT(bitmap != NULL, return 0);
    return bitmap->used_blocks;
}

blk_t reiserfs_bitmap_unused(reiserfs_bitmap_t *bitmap) {
    ASSERT(bitmap != NULL, return 0);

    ASSERT(bitmap->total_blocks - 
	bitmap->used_blocks > 0, return 0);
	
    return bitmap->total_blocks - bitmap->used_blocks;
}

int reiserfs_bitmap_check(reiserfs_bitmap_t *bitmap) {
    ASSERT(bitmap != NULL, return 0);
	
    if (reiserfs_bitmap_calc_used(bitmap) != bitmap->used_blocks)
	return 0;
	
    return 1;
}

reiserfs_bitmap_t *reiserfs_bitmap_alloc(blk_t len) {
    reiserfs_bitmap_t *bitmap;
	
    ASSERT(len > 0, goto error);
	
    if (!(bitmap = (reiserfs_bitmap_t *)libreiserfs_calloc(sizeof(*bitmap), 0)))
	goto error;
	
    bitmap->used_blocks = 0;
    bitmap->total_blocks = len;
    bitmap->size = (len + 7) / 8;
    
    if (!(bitmap->map = (char *)libreiserfs_calloc(bitmap->size, 0)))
	goto error_free_bitmap;
	
    return bitmap;
	
error_free_bitmap:
    reiserfs_bitmap_close(bitmap);
error:
    return NULL;
}

static int callback_bitmap_flush(dal_t *dal, 
    blk_t blk, char *map, uint32_t chunk, void *data) 
{
    reiserfs_block_t *block;
    reiserfs_bitmap_t *bitmap = (reiserfs_bitmap_t *)data;
	
    if (!(block = reiserfs_block_alloc(dal, blk, 0xff)))
	goto error;

    memcpy(block->data, map, chunk);

    /* Marking the rest of last byte of the bitmap as used */
    if ((map + chunk) - bitmap->map >= (long)bitmap->size) {
	uint32_t i, unused_bits = bitmap->size * 8 - bitmap->total_blocks;
	for (i = 0; i < unused_bits; i++) {
	    reiserfs_tools_set_bit((bitmap->total_blocks % 
		(dal_get_blocksize(dal) * 8)) + i, block->data);
	}
    }

    if (!reiserfs_block_write(dal, block)) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_OK, 
	    "Can't write bitmap block to %lu. %s.", blk, dal_error(dal));
	goto error_free_block;
    }
    reiserfs_block_free(block);
	
    return 1;
	
error_free_block:
    reiserfs_block_free(block);
error:
    return 0;
}

static int callback_bitmap_fetch(dal_t *dal, 
    blk_t blk, char *map, uint32_t chunk, void *data) 
{
    reiserfs_block_t *block;
    if (!(block = reiserfs_block_read(dal, blk))) {
	libreiserfs_exception_throw(EXCEPTION_ERROR, EXCEPTION_OK, 
	    "Can't read bitmap block %lu. %s.", blk, dal_error(dal));
	return 0;
    }

    memcpy(map, block->data, chunk);
    reiserfs_block_free(block);
	
    return 1;
}

int reiserfs_bitmap_pipe(reiserfs_bitmap_t *bitmap, 
    reiserfs_bitmap_pipe_func_t *pipe_func, void *data) 
{
    char *map;
    blk_t blk;
    uint32_t left, chunk;
	
    ASSERT(bitmap != NULL, return 0);
    ASSERT(bitmap->fs != NULL, return 0);
	
    map = bitmap->map;
    blk = bitmap->start;
    
    for (left = bitmap->size; left > 0; ) {
	chunk = (left < dal_get_blocksize(bitmap->fs->dal) ? left : 
	    dal_get_blocksize(bitmap->fs->dal));
	
	if (pipe_func && !pipe_func(bitmap->fs->dal, blk, map, chunk, data))
	    return 0;
		
	blk = (blk / (dal_get_blocksize(bitmap->fs->dal) * 8) + 1) * 
	    (dal_get_blocksize(bitmap->fs->dal) * 8);

	map += chunk;
	left -= chunk;
    }
	
    return 1;
}

reiserfs_bitmap_t *reiserfs_bitmap_open(reiserfs_fs_t *fs, 
    blk_t start, count_t len) 
{
    reiserfs_bitmap_t *bitmap;
    uint32_t i, unused_bits;

    ASSERT(fs != NULL, return NULL);
	
    if(!(bitmap = reiserfs_bitmap_alloc(len)))
	goto error;
	
    bitmap->start = start;
    bitmap->fs = fs;
	
    if (!reiserfs_bitmap_pipe(bitmap, callback_bitmap_fetch, (void *)bitmap))
	goto error_free_bitmap;

    unused_bits = bitmap->size * 8 - bitmap->total_blocks;
    
    for (i = 0; i < unused_bits; i++)
	reiserfs_tools_clear_bit(bitmap->total_blocks + i, bitmap->map);
    
    if (!(bitmap->used_blocks = reiserfs_bitmap_calc_used(bitmap)))
	goto error_free_bitmap;
	
    return bitmap;
	
error_free_bitmap:
    reiserfs_bitmap_close(bitmap);
error:
    return NULL;
}

reiserfs_bitmap_t *reiserfs_bitmap_create(reiserfs_fs_t *fs, 
    blk_t start, count_t len) 
{
    blk_t i, bmap_blknr;
    reiserfs_bitmap_t *bitmap;
    
    ASSERT(fs != NULL, return NULL);

    if (!(bitmap = reiserfs_bitmap_alloc(len)))
	return NULL;
	
    bitmap->start = start;
    bitmap->fs = fs;
    
    /* Marking first bitmap block as used */
    reiserfs_bitmap_use_block(bitmap, start);
  
    /* Setting up other bitmap blocks */
    bmap_blknr = (len - 1) / (dal_get_blocksize(fs->dal) * 8) + 1;

    for (i = 1; i < bmap_blknr; i++)
	reiserfs_bitmap_use_block(bitmap, i * dal_get_blocksize(fs->dal) * 8);

    return bitmap;
}

static uint32_t reiserfs_bitmap_resize_map(reiserfs_bitmap_t *bitmap, 
    long start, long end, uint16_t blocksize) 
{
    char *map;
    long i, right, journal_len, offset;
    long size = ((end - start) + 7) / 8;
	
    if (start == 0) {
	int chunk;
		
	if (size == (long)bitmap->size)
	    return bitmap->size;
		
  	if (!libreiserfs_realloc((void **)&bitmap->map, size))
	    return 0;

  	if ((chunk = size - bitmap->size) > 0)
	    memset(bitmap->map + bitmap->size, 0, chunk);

	return size;
    }

    if (!(map = libreiserfs_calloc(size, 0)))
	return 0;

    journal_len = get_jp_len(get_sb_jp(bitmap->fs->super));
    offset = bitmap->fs->super_off + 1 + journal_len;
	    
    memcpy(map, bitmap->map, (offset / 8) + 1);
   
    right = end > (long)bitmap->total_blocks ?
	    (long)bitmap->total_blocks : end;
    
    if (start < 0) {
	for (i = right - 1; i >= offset + 1; i--) {
	    if (reiserfs_tools_test_bit(i, bitmap->map)) {
		if (i + start > offset + 1)
		    reiserfs_tools_set_bit(i + start, map);
	    }
	}
    } else {
	for (i = start + offset + 1; i < right; i++) {
	    if (reiserfs_tools_test_bit(i, bitmap->map))
		reiserfs_tools_set_bit(i - start, map);
	}
    }
	
    libreiserfs_free(bitmap->map);
    bitmap->map = map;
	
    return size;
}

int reiserfs_bitmap_resize(reiserfs_bitmap_t *bitmap, 
    long start, long end) 
{
    int size;
    blk_t i, bmap_old_blknr, bmap_new_blknr;
	
    ASSERT(bitmap != NULL, return 0);
    ASSERT(end - start > 0, return 0);

    if ((size = reiserfs_bitmap_resize_map(bitmap, start, 
	    end, dal_get_blocksize(bitmap->fs->dal))) - bitmap->size == 0)
	return 1;

    bmap_old_blknr = bitmap->size / dal_get_blocksize(bitmap->fs->dal);
    
    bmap_new_blknr = (end - start - 1) / 
	(dal_get_blocksize(bitmap->fs->dal) * 8) + 1;

    bitmap->size = size;
    bitmap->total_blocks = end - start;
	
    /* Marking new bitmap blocks as used */
    if (bmap_new_blknr - bmap_old_blknr > 0) {
	for (i = bmap_old_blknr; i < bmap_new_blknr; i++)
	    reiserfs_bitmap_use_block(bitmap, i * dal_get_blocksize(bitmap->fs->dal) * 8);
    }

    return 1;
}

blk_t reiserfs_bitmap_copy(reiserfs_bitmap_t *dest_bitmap, 
    reiserfs_bitmap_t *src_bitmap, blk_t len) 
{
	
    ASSERT(dest_bitmap != NULL, return 0);
    ASSERT(src_bitmap != NULL, return 0);

    if (!len) 
	return 0;
	
    if (!reiserfs_bitmap_resize(dest_bitmap, 0, (len > src_bitmap->total_blocks ? 
	    src_bitmap->total_blocks : len)))
        return 0;
	
    memcpy(dest_bitmap->map, src_bitmap->map, dest_bitmap->size);
    dest_bitmap->used_blocks = reiserfs_bitmap_used(dest_bitmap);

    return dest_bitmap->total_blocks;
}

reiserfs_bitmap_t *reiserfs_bitmap_clone(reiserfs_bitmap_t *bitmap) {
    reiserfs_bitmap_t *clone;

    ASSERT(bitmap != NULL, return 0);	

    if (!(clone = reiserfs_bitmap_alloc(bitmap->total_blocks)))
	return NULL;
	
    memcpy(clone->map, bitmap->map, clone->size);
    clone->used_blocks = reiserfs_bitmap_used(clone);
	
    return clone;
}

int reiserfs_bitmap_sync(reiserfs_bitmap_t *bitmap) {

    if (!reiserfs_bitmap_pipe(bitmap, callback_bitmap_flush, (void *)bitmap))
	return 0;

    return 1;
}

void reiserfs_bitmap_close(reiserfs_bitmap_t *bitmap) {
    ASSERT(bitmap != NULL, return);
	
    if (bitmap->map)
	libreiserfs_free(bitmap->map);

    libreiserfs_free(bitmap);
}

reiserfs_bitmap_t *reiserfs_bitmap_reopen(reiserfs_bitmap_t *bitmap) {
    blk_t start;
    count_t len;
    reiserfs_fs_t *fs;
	
    ASSERT(bitmap != NULL, return NULL);

    fs = bitmap->fs;
    start = bitmap->start;
    len = bitmap->total_blocks;
	    
    reiserfs_bitmap_close(bitmap);

    return reiserfs_bitmap_open(fs, start, len);
}

char *reiserfs_bitmap_map(reiserfs_bitmap_t *bitmap) {
    ASSERT(bitmap != NULL, return NULL);
    return bitmap->map;
}

