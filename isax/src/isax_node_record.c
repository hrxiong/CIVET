//
//  isax_node_record.c
//  isaxlib
//
//  Created by Kostas Zoumpatianos on 3/11/12.
//  Copyright 2012 University of Trento. All rights reserved.
//
#include "config.h"
#include "globals.h"
#include <stdio.h>
#include <stdlib.h>
#include "isax_node_record.h"

isax_node_record *isax_node_record_init(int sax_size, int window_size)
{
    isax_node_record *node = malloc(sizeof(isax_node_record));

    node->window_size = window_size;

    node->sax = malloc(sizeof(sax_type) * sax_size);
    node->sax_lower = malloc(sizeof(sax_type) * sax_size);
    node->sax_upper = malloc(sizeof(sax_type) * sax_size);

    node->start_pos = malloc(sizeof(file_position_type) * window_size);
    node->min_len = malloc(sizeof(int) * window_size);

    return node;
}

void isax_node_record_destroy(isax_node_record *node)
{
    free(node->sax);
    free(node->sax_lower);
    free(node->sax_upper);

    free(node->start_pos);
    free(node->min_len);

    free(node);
}
