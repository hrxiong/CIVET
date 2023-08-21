//
//  isax_node_record.h
//  isaxlib
//
//  Created by Kostas Zoumpatianos on 3/10/12.
//  Copyright 2012 University of Trento. All rights reserved.
//

#ifndef isaxlib_isax_node_record_h
#define isaxlib_isax_node_record_h
#include "config.h"
#include "globals.h"

typedef struct
{
    sax_type *sax;
    sax_type *sax_upper;
    sax_type *sax_lower;

    int window_size;

    file_position_type *start_pos;
    int *min_len;

} isax_node_record;

// #ifdef __cplusplus extern "C" {
// #endif
isax_node_record *isax_node_record_init(int sax_size, int window_size);
void isax_node_record_destroy(isax_node_record *node);
// #ifdef __cplusplus }
// #endif

#endif
