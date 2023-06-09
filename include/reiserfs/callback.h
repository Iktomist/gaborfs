#ifndef CALLBACK_H
#define CALLBACK_H

#include "segment.h"
#include "block.h"
#include "gauge.h"

int reiserfs_callback_segment_gauge(reiserfs_segment_t *segment, reiserfs_block_t *block, 
    long number, reiserfs_gauge_t *gauge);

#endif

