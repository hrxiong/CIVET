//
//  isax_node_buffer.c
//  aisax
//
//  Created by Kostas Zoumpatianos on 4/6/12.
//  Copyright 2012 University of Trento. All rights reserved.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "globals.h"
#include "isax_node_buffer.h"
#include "isax_node.h"
#include "isax_node_record.h"
#include "proxy_function.h"

enum response add_to_node_buffer(isax_node *node, isax_node_record *record,
                                 int sax_size, int ts_size, int initial_buffer_size)
{

    if (!node->is_leaf)
    {
        fprintf(stderr, "sanity error: Adding to non-leaf node!\n");
    }

    // COPY DATA TO BUFFER

    char full_add;

    // Add both sax representation and time series in buffers
    if (record->sax != NULL && record->start_pos != NULL)
    {
        full_add = 1;
        // fprintf(stderr,"BUF\t%lld\n", record->position);
    }
    // Only add sax representation which comes from a parent
    else if (record->sax != NULL && record->start_pos == NULL)
    {
        full_add = 0;
    }
    else
    {
        fprintf(stderr, "error: no SAX representation received as input!");
        exit(-1);
    }

    if (full_add)
    {
        update_node_sax_lower_upper(node, record->sax_lower, record->sax_upper, sax_size);

        // Sax Buffer
        if (node->max_sax_buffer_size == 0)
        {
            // ALLOCATE MEMORY.
            node->max_sax_buffer_size = initial_buffer_size;
            node->sax_buffer = (sax_type **)malloc(sizeof(sax_type *) * node->max_sax_buffer_size);

            if (node->sax_buffer == NULL)
            {
                fprintf(stderr, "error: could not allocate memory for node sax buffer.\n");
                return OUT_OF_MEMORY_FAILURE;
            }
            // END MEMORY ALLOCATION
        }
        else if (node->max_sax_buffer_size <= node->sax_buffer_size)
        {
            // REALLOCATE MEMORY.
            node->max_sax_buffer_size *= BUFFER_REALLOCATION_RATE;

            node->sax_buffer = (sax_type **)realloc(node->sax_buffer,
                                                    sizeof(sax_type *) * node->max_sax_buffer_size);

            if (node->sax_buffer == NULL)
            {
                fprintf(stderr, "error: could not allocate memory for node sax buffer.\n");
                return OUT_OF_MEMORY_FAILURE;
            }
            // END MEMORY REALLOCATION
        }

        // Time series Buffer
        if (node->max_env_buffer_size == 0)
        {
            // ALLOCATE MEMORY.
            node->max_env_buffer_size = initial_buffer_size;
            node->sax_lower_buffer = (sax_type **)malloc(sizeof(sax_type *) * node->max_env_buffer_size);
            node->sax_upper_buffer = (sax_type **)malloc(sizeof(sax_type *) * node->max_env_buffer_size);
            node->start_pos_buffer = (file_position_type **)malloc(sizeof(file_position_type *) * node->max_env_buffer_size);
            node->min_len_buffer = (int **)malloc(sizeof(int *) * node->max_env_buffer_size);
            node->window_size_buffer = (int *)malloc(sizeof(int) * node->max_env_buffer_size);
            // node->ts_buffer = (ts_type**) malloc(sizeof(ts_type*) * node->max_env_buffer_size);
            // node->pos_buffer = (file_position_type*) malloc(sizeof(file_position_type) * node->max_env_buffer_size);

            if (node->sax_upper_buffer == NULL)
            {
                fprintf(stderr, "error: could not allocate memory for node ts buffer.\n");
                return OUT_OF_MEMORY_FAILURE;
            }
        }
        else if (node->max_env_buffer_size <= node->env_buffer_size)
        {
            // REALLOCATE MEMORY.
#ifdef HARDLBL
            int prev_ts_buffer_size = node->max_ts_buffer_size;
#endif
            node->max_env_buffer_size *= BUFFER_REALLOCATION_RATE;
            node->sax_lower_buffer = (sax_type **)realloc(node->sax_lower_buffer,
                                                          sizeof(sax_type *) * node->max_env_buffer_size);
            node->sax_upper_buffer = (sax_type **)realloc(node->sax_upper_buffer,
                                                          sizeof(sax_type *) * node->max_env_buffer_size);
            node->start_pos_buffer = (file_position_type **)realloc(node->start_pos_buffer,
                                                                    sizeof(file_position_type *) * node->max_env_buffer_size);
            node->min_len_buffer = (int **)realloc(node->min_len_buffer,
                                                   sizeof(int *) * node->max_env_buffer_size);
            node->window_size_buffer = (int *)realloc(node->window_size_buffer,
                                                      sizeof(int) * node->max_env_buffer_size);

            if (node->sax_upper_buffer == NULL)
            {
                fprintf(stderr, "error: could not allocate memory for node ts buffer.\n");
                return OUT_OF_MEMORY_FAILURE;
            }
            // END MEMORY REALLOCATION
        }
    }
    else
    {
        if (node->max_prev_sax_buffer_size == 0)
        {
            // ALLOCATE MEMORY.
            node->max_prev_sax_buffer_size = initial_buffer_size;
            node->prev_sax_buffer = (sax_type **)malloc(sizeof(sax_type *) * node->max_prev_sax_buffer_size);
            if (node->prev_sax_buffer == NULL)
            {
                fprintf(stderr, "error: could not allocate memory for node sax buffer.\n");
                return OUT_OF_MEMORY_FAILURE;
            }
            // END MEMORY ALLOCATION
        }
        else if (node->max_prev_sax_buffer_size <= node->prev_sax_buffer_size)
        {
            // REALLOCATE MEMORY.
            node->max_prev_sax_buffer_size *= BUFFER_REALLOCATION_RATE;

            node->prev_sax_buffer = (sax_type **)realloc(node->prev_sax_buffer,
                                                         sizeof(sax_type *) * node->max_prev_sax_buffer_size);
            if (node->prev_sax_buffer == NULL)
            {
                fprintf(stderr, "error: could not allocate memory for node sax buffer.\n");
                return OUT_OF_MEMORY_FAILURE;
            }
        }
    }

    // Add data in buffers
    if (full_add)
    {
        node->sax_buffer[node->sax_buffer_size] = record->sax;
        node->sax_buffer_size++;

        node->sax_lower_buffer[node->env_buffer_size] = record->sax_lower;

        node->sax_upper_buffer[node->env_buffer_size] = record->sax_upper;
        node->start_pos_buffer[node->env_buffer_size] = record->start_pos;
        node->min_len_buffer[node->env_buffer_size] = record->min_len;
        node->window_size_buffer[node->env_buffer_size] = record->window_size;
        // printf("add to node with window size %d\n", record->window_size);

        node->env_buffer_size++;
    }
    else
    {
        node->prev_sax_buffer[node->prev_sax_buffer_size] = record->sax;
        node->prev_sax_buffer_size++;
    }
    node->leaf_size++;

    return SUCCESS;
}

enum response flush_node_buffer(isax_node *node, isax_index_settings *settings)
{
    if (node->sax_buffer_size > 0 || node->env_buffer_size > 0 || node->prev_sax_buffer_size > 0)
    {
        int i;

        if (node->filename == NULL)
        {
            fprintf(stderr, "I do not have a node filename");
            exit(-1);
            return FAILURE;
        }

        if (node->env_buffer_size > 0)
        {
            char *fname = malloc(sizeof(char) * (strlen(node->filename) + 4));
            strcpy(fname, node->filename);
            strcat(fname, ".ts");

            COUNT_PARTIAL_RAND_OUTPUT
            COUNT_PARTIAL_OUTPUT_TIME_START
            FILE *ts_file = fopen(fname, "a+");

            write_node_buffer_into_file(ts_file, node, settings);

            // for (i=0; i<node->env_buffer_size; i++) {
            //     COUNT_PARTIAL_SEQ_OUTPUT
            //     COUNT_PARTIAL_SEQ_OUTPUT

            //     fwrite(node->ts_buffer[i], sizeof(ts_type), settings->timeseries_size, ts_file);
            //     fwrite(&node->pos_buffer[i], sizeof(file_position_type), 1, ts_file);
            //     //file_position_type a = -3;
            //     //fwrite(&a, sizeof(file_position_type), 1, ts_file);
            // }

            free(fname);
            fclose(ts_file);
            COUNT_PARTIAL_OUTPUT_TIME_END
        }

        if (node->sax_buffer_size > 0)
        {
            char *fname = malloc(sizeof(char) * (strlen(node->filename) + 5));
            strcpy(fname, node->filename);
            strcat(fname, ".sax");
            COUNT_PARTIAL_RAND_OUTPUT
            COUNT_PARTIAL_OUTPUT_TIME_START
            FILE *sax_file = fopen(fname, "a+");

            for (i = 0; i < node->sax_buffer_size; i++)
            {
                fwrite(node->sax_buffer[i], sizeof(sax_type), settings->paa_segments, sax_file);
                // sax_print(node->sax_buffer[i], index->settings->paa_segments, index->settings->sax_bit_cardinality);
            }

            free(fname);
            fclose(sax_file);
            COUNT_PARTIAL_OUTPUT_TIME_END
        }

        if (node->prev_sax_buffer_size > 0)
        {
            char *fname = malloc(sizeof(char) * (strlen(node->filename) + 5));
            strcpy(fname, node->filename);
            strcat(fname, ".pre");
            COUNT_PARTIAL_RAND_OUTPUT
            COUNT_OUTPUT_TIME_START
            FILE *psax_file = fopen(fname, "a+");

            for (i = 0; i < node->prev_sax_buffer_size; i++)
            {
                COUNT_PARTIAL_SEQ_OUTPUT
                fwrite(node->prev_sax_buffer[i], sizeof(sax_type), settings->paa_segments, psax_file);
            }

            free(fname);
            fclose(psax_file);
            COUNT_OUTPUT_TIME_END
        }
    }
    return SUCCESS;
}

enum response clear_node_buffer(isax_node *node, const char full_clean)
{
    int i;

    for (i = 0; i < node->prev_sax_buffer_size; i++)
    {
        free(node->prev_sax_buffer[i]);
    }

    if (full_clean)
    {
        for (i = 0; i < node->sax_buffer_size; i++)
        {
            free(node->sax_buffer[i]);
        }
        for (i = 0; i < node->env_buffer_size; i++)
        {
            free(node->sax_lower_buffer[i]);
            free(node->sax_upper_buffer[i]);
            free(node->start_pos_buffer[i]);
            free(node->min_len_buffer[i]);
        }
        // for (i=0; i<node->ts_buffer_size; i++) {
        //     free(node->ts_buffer[i]);
        // }
    }

    node->sax_buffer_size = 0;
    node->env_buffer_size = 0;
    node->prev_sax_buffer_size = 0;

    if (node->max_sax_buffer_size > 0)
    {
        free(node->sax_buffer);
        node->max_sax_buffer_size = 0;
    }
    if (node->max_env_buffer_size > 0)
    {
        free(node->sax_lower_buffer);
        free(node->sax_upper_buffer);
        free(node->window_size_buffer);
        free(node->min_len_buffer);
        free(node->start_pos_buffer);
        node->max_env_buffer_size = 0;
    }
    if (node->max_prev_sax_buffer_size > 0)
    {
        free(node->prev_sax_buffer);
        node->max_prev_sax_buffer_size = 0;
    }

    return SUCCESS;
}
