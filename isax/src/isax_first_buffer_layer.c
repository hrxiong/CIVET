//
//  first_buffer_layer.c
//  isaxlib
//
//  Created by Kostas Zoumpatianos on 3/20/12.
//  Copyright 2012 University of Trento. All rights reserved.
//
//
//  Modified by Karima Echihabi on 09/08/17 to improve memory management.
//  Copyright 2017 Paris Descartes University. All rights reserved.
//

#include "config.h"
#include "globals.h"
#include <stdio.h>
#include <stdlib.h>
#include "sax/sax.h"
#include "isax_first_buffer_layer.h"

// TODO: CONVERT SOFT FBLS HARD REPOSITORY TO BE A GIANT ARRAY OF BYTES.
struct first_buffer_layer *initialize_fbl(int initial_buffer_size, int number_of_buffers,
                                          unsigned long long max_total_size, isax_index *index)
{
    struct first_buffer_layer *fbl = malloc(sizeof(first_buffer_layer));

    fbl->max_total_size = max_total_size; //+

    fbl->initial_buffer_size = initial_buffer_size;
    fbl->number_of_buffers = number_of_buffers;

    unsigned long long hard_buffer_size = (unsigned long long)(index->settings->full_record_size) * max_total_size;
    fbl->hard_buffer = malloc(hard_buffer_size);

    if (fbl->hard_buffer == NULL)
    {
        fprintf(stderr, "Could not initialize hard buffer of size: %llu\n", hard_buffer_size);
        exit(-1);
    }

    fbl->buffers = malloc(sizeof(first_buffer_layer_node) * number_of_buffers);
    if (fbl->buffers == NULL)
    {
        fprintf(stderr, "Could not initialize buffers%llu\n");
        exit(-1);
    }

    fbl->current_record_index = 0;
    fbl->current_record = fbl->hard_buffer;

    int i;
    for (i = 0; i < number_of_buffers; i++)
    {
        fbl->buffers[i].initialized = 0;
    }

    /*
    fbl->internal_node_buffer = malloc(index->settings->max_leaf_size *  sizeof(isax_node_record));
    if(fbl->internal_node_buffer == NULL) {
    fprintf(stderr, "Could not initialize the internal node buffer%llu\n");
    exit(-1);
    }

    fbl->internal_node_buffer_size = index->settings->max_leaf_size;
    for (i=0; i < fbl->internal_node_buffer_size; ++i)
    {
       fbl->internal_node_buffer[i].sax = calloc(index->settings->paa_segments, sizeof(sax_type));
       fbl->internal_node_buffer[i].ts = calloc(index->settings->timeseries_size, sizeof(ts_type));
    }
    */

    return fbl;
}

isax_node *insert_to_fbl(first_buffer_layer *fbl, isax_node_record *record,
                         root_mask_type mask, isax_index *index)
{

    first_buffer_layer_node *current_fbl_node = &fbl->buffers[(int)mask];

    // ############ CREATE NEW FBL ###########
    if (!current_fbl_node->initialized)
    {

        current_fbl_node->initialized = 1;
        current_fbl_node->max_buffer_size = 0;
        current_fbl_node->buffer_size = 0;

        current_fbl_node->node = isax_root_node_init(mask, index->settings->paa_segments,
                                                     index->settings->timeseries_size);
        index->root_nodes++;
        current_fbl_node->node->is_leaf = 1;

        if (index->first_node == NULL)
        {
            index->first_node = current_fbl_node->node;
            current_fbl_node->node->next = NULL;
            current_fbl_node->node->previous = NULL;
        }
        else
        {
            isax_node *prev_first = index->first_node;
            index->first_node = current_fbl_node->node;
            index->first_node->next = prev_first;
            prev_first->previous = current_fbl_node->node;
        }
    }

    if (current_fbl_node->buffer_size >= current_fbl_node->max_buffer_size)
    {

        if (current_fbl_node->max_buffer_size == 0)
        {
            current_fbl_node->max_buffer_size = fbl->initial_buffer_size;

            // current_fbl_node->ts_buffer = malloc(sizeof(ts_type *) *
            //                                      current_fbl_node->max_buffer_size);
            current_fbl_node->sax_buffer = malloc(sizeof(sax_type *) *
                                                  current_fbl_node->max_buffer_size);

            current_fbl_node->sax_upper_buffer = malloc(sizeof(sax_type *) *
                                                        current_fbl_node->max_buffer_size);

            current_fbl_node->sax_lower_buffer = malloc(sizeof(sax_type *) *
                                                        current_fbl_node->max_buffer_size);


            current_fbl_node->start_pos_buffer = malloc(sizeof(file_position_type *) *
                                                        current_fbl_node->max_buffer_size);
            current_fbl_node->min_len_buffer = malloc(sizeof(int *) *
                                                      current_fbl_node->max_buffer_size);
            current_fbl_node->window_size_buffer = malloc(sizeof(int) *
                                                          current_fbl_node->max_buffer_size);
        }
        else
        {
            current_fbl_node->max_buffer_size *= BUFFER_REALLOCATION_RATE;

            // current_fbl_node->ts_buffer = realloc(current_fbl_node->ts_buffer,
            //                                       sizeof(ts_type *) *
            //                                       current_fbl_node->max_buffer_size);
            current_fbl_node->sax_buffer = realloc(current_fbl_node->sax_buffer,
                                                   sizeof(sax_type *) *
                                                       current_fbl_node->max_buffer_size);
            current_fbl_node->sax_upper_buffer = realloc(current_fbl_node->sax_upper_buffer,
                                                         sizeof(sax_type *) *
                                                             current_fbl_node->max_buffer_size);
            current_fbl_node->sax_lower_buffer = realloc(current_fbl_node->sax_lower_buffer,
                                                         sizeof(sax_type *) *
                                                             current_fbl_node->max_buffer_size);
            current_fbl_node->start_pos_buffer = realloc(current_fbl_node->start_pos_buffer,
                                                         sizeof(file_position_type *) *
                                                             current_fbl_node->max_buffer_size);

            current_fbl_node->min_len_buffer = realloc(current_fbl_node->min_len_buffer,
                                                       sizeof(int *) *
                                                           current_fbl_node->max_buffer_size);

            current_fbl_node->window_size_buffer = realloc(current_fbl_node->window_size_buffer,
                                                           sizeof(int) *
                                                               current_fbl_node->max_buffer_size);

            // current_fbl_node->position_buffer = realloc(current_fbl_node->position_buffer, sizeof(file_position_type *) * current_fbl_node->max_buffer_size);
        }
    }

    /*
    if (current_fbl_node->buffer_size >= current_fbl_node->max_buffer_size) {

        if(current_fbl_node->max_buffer_size == 0) {
            current_fbl_node->max_buffer_size = fbl->initial_buffer_size;

            current_fbl_node->max_buffer_size = fbl->initial_buffer_size;

            current_fbl_node->ts_buffer = malloc(sizeof(ts_type) * index->settings->timeseries_size *
                                                 current_fbl_node->max_buffer_size);
            current_fbl_node->sax_buffer = malloc(sizeof(sax_type) * index->settings->paa_segments *
                                                  current_fbl_node->max_buffer_size);
            current_fbl_node->position_buffer = malloc(sizeof(file_position_type) * current_fbl_node->max_buffer_size);
        }
        else {
            current_fbl_node->max_buffer_size *= BUFFER_REALLOCATION_RATE;

            current_fbl_node->ts_buffer = realloc(current_fbl_node->ts_buffer,
                                                  sizeof(ts_type) * index->settings->timeseries_size *
                                                  current_fbl_node->max_buffer_size);
            current_fbl_node->sax_buffer = realloc(current_fbl_node->sax_buffer,
                                                   sizeof(sax_type) * index->settings->paa_segments *
                                                   current_fbl_node->max_buffer_size);
            current_fbl_node->position_buffer = realloc(current_fbl_node->position_buffer, sizeof(file_position_type) * current_fbl_node->max_buffer_size);
        }
    }
    */

    if (current_fbl_node->sax_buffer == NULL)
    {
        fprintf(stderr, "error: Could not allocate memory for sax representations in FBL.");
        return OUT_OF_MEMORY_FAILURE;
    }
    // if (current_fbl_node->ts_buffer == NULL) {
    //     fprintf(stderr, "error: Could not allocate memory for ts representations in FBL.");
    //     return OUT_OF_MEMORY_FAILURE;
    // }
    // /* START+*/
    // if (current_fbl_node->position_buffer == NULL) {
    //     fprintf(stderr, "error: Could not allocate memory for pos representations in FBL.");
    //     return OUT_OF_MEMORY_FAILURE;
    // }
    /* END+*/

    /*
    unsigned long long i;
    unsigned long long  j = 0;
    for (i = index->settings->paa_segments * current_fbl_node->buffer_size;
         i < (index->settings->paa_segments * current_fbl_node->buffer_size)
         + index->settings->paa_segments; i++) {
        current_fbl_node->sax_buffer[i] = record->sax[j];
        j++;
    }

    j = 0;
    //printf("OFFSET: %d\n",  index->settings->timeseries_size * current_fbl_node->buffer_size);
    for (i = index->settings->timeseries_size * current_fbl_node->buffer_size;
         i < (index->settings->timeseries_size * current_fbl_node->buffer_size)
         + index->settings->timeseries_size; i++) {
        current_fbl_node->ts_buffer[i] = record->ts[j];
        j++;
    }
    current_fbl_node->position_buffer[current_fbl_node->buffer_size] = record->position;
    */

    /* START+ */
    current_fbl_node->sax_buffer[current_fbl_node->buffer_size] = (sax_type *)fbl->current_record;
    memcpy((void *)(fbl->current_record), (void *)(record->sax), index->settings->sax_byte_size);
    fbl->current_record += index->settings->sax_byte_size;

    current_fbl_node->sax_upper_buffer[current_fbl_node->buffer_size] = (sax_type *)fbl->current_record;
    memcpy((void *)(fbl->current_record), (void *)(record->sax_upper), index->settings->sax_upper_byte_size);
    fbl->current_record += index->settings->sax_upper_byte_size;

    current_fbl_node->sax_lower_buffer[current_fbl_node->buffer_size] = (sax_type *)fbl->current_record;
    memcpy((void *)(fbl->current_record), (void *)(record->sax_lower), index->settings->sax_lower_byte_size);
    fbl->current_record += index->settings->sax_lower_byte_size;

    current_fbl_node->start_pos_buffer[current_fbl_node->buffer_size] = (file_position_type *)fbl->current_record;
    memcpy((void *)(fbl->current_record), (void *)(record->start_pos), index->settings->start_pos_byte_size);
    fbl->current_record += index->settings->start_pos_byte_size;

    current_fbl_node->min_len_buffer[current_fbl_node->buffer_size] = (int *)fbl->current_record;
    memcpy((void *)(fbl->current_record), (void *)(record->min_len), index->settings->min_len_byte_size);
    fbl->current_record += index->settings->min_len_byte_size;

    current_fbl_node->window_size_buffer[current_fbl_node->buffer_size] = (int)record->window_size;
    // memcpy((void *)(fbl->current_record), (void *)&(record->window_size), index->settings->window_size_byte_size);
    // printf("insert data into first buffer layer with window size %d\n", current_fbl_node->window_size_buffer[current_fbl_node->buffer_size]);
    // printf("origin data %d %d\n", record->window_size, index->settings->window_size_byte_size);
    // fbl->current_record += index->settings->window_size_byte_size;

    // current_fbl_node->ts_buffer[current_fbl_node->buffer_size] = (ts_type *) fbl->current_record;
    // memcpy((void *) (fbl->current_record), (void *) (record->ts), index->settings->ts_byte_size);
    // fbl->current_record += index->settings->ts_byte_size;

    // current_fbl_node->position_buffer[current_fbl_node->buffer_size] = (file_position_type *) fbl->current_record;
    // //*(fbl->current_record) = (file_position_type) record->position;
    // memcpy(fbl->current_record, &(record->position), index->settings->position_byte_size);
    // fbl->current_record += index->settings->position_byte_size;

    current_fbl_node->buffer_size++;
    /* START+ */
    fbl->current_record_index++;
    /* END+ */

    return current_fbl_node->node;
}

enum response flush_fbl(first_buffer_layer *fbl, isax_index *index)
{

    unsigned long long c = 1;
    unsigned long long j;
    isax_node_record *r = malloc(sizeof(isax_node_record));
    for (j = 0; j < fbl->number_of_buffers; j++)
    {

        first_buffer_layer_node *current_fbl_node = &index->fbl->buffers[j];

        if (!current_fbl_node->initialized)
        {
            continue;
        }

        unsigned long long i;
        if (current_fbl_node->buffer_size > 0)
        {
            // For all records in this buffer
            for (i = 0; i < current_fbl_node->buffer_size; i++)
            {
                /*
                    r->sax = &current_fbl_node->sax_buffer[i*index->settings->paa_segments];
                    r->ts = &current_fbl_node->ts_buffer[i*index->settings->timeseries_size];
                    r->position = current_fbl_node->position_buffer[i];
                    */

                /*  START+ */
                r->sax = (sax_type *)current_fbl_node->sax_buffer[i];
                r->sax_upper = (sax_type *)current_fbl_node->sax_upper_buffer[i];
                r->sax_lower = (sax_type *)current_fbl_node->sax_lower_buffer[i];

                r->start_pos = (file_position_type *)current_fbl_node->start_pos_buffer[i];
                r->min_len = (int *)current_fbl_node->min_len_buffer[i];
                r->window_size = (int)current_fbl_node->window_size_buffer[i];
                // r->ts = (ts_type *) current_fbl_node->ts_buffer[i];
                // r->position = (file_position_type) current_fbl_node->position_buffer[i];
                /*  END+ */

                // printf("The position that I am inserting is: %lld\n", r->position);
                // fprintf(stderr,"FLUSH\t%lld\n", r->position);

                // Add record to index
                // printf("Test before add_record_to_node\n");
                add_record_to_node(index, current_fbl_node->node, r, 1);
                // printf("Test after add_record_to_node\n");
            }
            // printf("Test before flush_subtree_leaf_buffers\n");
            // flush index node
            flush_subtree_leaf_buffers(index, current_fbl_node->node);
            // printf("Test after flush_subtree_leaf_buffers\n");

            // clear FBL records moved in LBL buffers
            free(current_fbl_node->sax_buffer);
            free(current_fbl_node->sax_upper_buffer);
            free(current_fbl_node->sax_lower_buffer);
            free(current_fbl_node->start_pos_buffer);
            free(current_fbl_node->min_len_buffer);
            free(current_fbl_node->window_size_buffer);

            // printf("Test before isax_index_clear_node_buffers\n");
            // clear records read from files (free only prev sax buffers)
            isax_index_clear_node_buffers(index, current_fbl_node->node, 1, 0);
            // printf("Test after isax_index_clear_node_buffers\n");

            // Set to 0 in order to re-allocate original space for buffers.
            current_fbl_node->buffer_size = 0;
            current_fbl_node->max_buffer_size = 0;
        }
    }
    free(r);
    fbl->current_record_index = 0;
    fbl->current_record = fbl->hard_buffer;

    return SUCCESS;
}

void destroy_fbl(first_buffer_layer *fbl, int use_index)
{
    if (use_index)
        free(fbl->hard_buffer);


    if(fbl->buffers)
        free(fbl->buffers);
    /*for (int i=0; i < fbl->internal_node_buffer_size; ++i)
    {
       free(fbl->internal_node_buffer[i].sax);
       free(fbl->internal_node_buffer[i].ts);
    }
    free(fbl->internal_node_buffer);
    */
    free(fbl);
}
