//
//  isax_index.c
//  isaxlib
//
//  Created by Kostas Zoumpatianos on 3/10/12.
//  Copyright 2012 University of Trento. All rights reserved.
//
//  Modified by Karima Echihabi on 09/08/17 to improve memory management.
//  Copyright 2017 Paris Descartes University. All rights reserved.
//

/*
 ============= NOTES: =============
 Building a mask for the following sax word:
 SAX:
 00
 00
 01
 00
 11
 01
 10
 11

 How to build a mask for the FIRST bit of this word (root),
 I use do:
 R = 00000000
 IF(00 AND 10) R = R OR 10000000
 IF(00 AND 10) R = R OR 01000000
 IF(01 AND 10) R = R OR 00100000
 IF(00 AND 10) R = R OR 00010000
 IF(11 AND 10) R = R OR 00001000
 IF(01 AND 10) R = R OR 00000100
 IF(10 AND 10) R = R OR 00000010
 IF(11 AND 10) R = R OR 00000001
 result: R = 00001011


 *** IN ORDER TO CALCULATE LOCATION BITMAP MASKS ***:

 m = 2^NUMBER_OF_MASKS     (e.g for 2^3=8 = 100)
 m>> for the second mask   (e.g.            010)
 m>>>> for the third ...   (e.g.            001)
*/
#include "config.h"
#include "globals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include "sax/sax.h"
#include "isax_index.h"
#include "isax_node.h"
#include "isax_node_buffer.h"
#include "isax_node_split.h"
#include "isax_first_buffer_layer.h"
#include "isax_query_engine.h"
#include "proxy_function.h"

query_result *cmpfunc(const void *a, const void *b);

/**
 This function initializes the settings of an isax index
 */
isax_index_settings *isax_index_settings_init(const char *root_directory, int timeseries_size,
                                              int paa_segments, int sax_bit_cardinality,
                                              int max_leaf_size, int initial_leaf_buffer_size,
                                              unsigned long long max_total_buffer_size, int initial_fbl_buffer_size,
                                              int new_index, int window_size,
                                              int minl, int maxl, int W, int H)
{

    isax_index_settings *settings = malloc(sizeof(isax_index_settings));
    if (settings == NULL)
    {
        fprintf(stderr, "error: could not allocate memory for index settings.\n");
        return NULL;
    }

    // if (new_index)
    // {
    //     if (chdir(root_directory) == 0)
    //     {
    //         fprintf(stderr, "WARNING! Target index directory already exists. Please delete or choose a new one.\n");
    //     }
    //     mkdir(root_directory, 0777);
    // }
    // else
    // {
    //     if (chdir(root_directory) != 0)
    //     {
    //         fprintf(stderr, "WARNING! Target index directory does not exist!\n");
    //     }
    //     else
    //     {
    //         chdir("../");
    //     }
    // }

    if (paa_segments > (int)(8 * (int)sizeof(root_mask_type)))
    {
        fprintf(stderr, "error: Too many paa segments. The maximum value is %zu.\n",
                8 * sizeof(root_mask_type));
        return NULL;
    }

    if (initial_leaf_buffer_size < max_leaf_size)
    {
        fprintf(stderr, "error: Leaf buffers should be at least as big as leafs.\n");
        return NULL;
    }

    settings->root_directory = root_directory;
    settings->initial_fbl_buffer_size = initial_fbl_buffer_size;
    settings->timeseries_size = timeseries_size;
    settings->paa_segments = paa_segments;
    settings->ts_values_per_paa_segment = timeseries_size / paa_segments;
    settings->max_leaf_size = max_leaf_size;
    settings->initial_leaf_buffer_size = initial_leaf_buffer_size;

    double actual_size = max_total_buffer_size * 0.95;

    settings->max_total_buffer_size = (unsigned long long)actual_size;

    settings->sax_byte_size = (sizeof(sax_type) * paa_segments);

    // HRTODO
    //////////////
    settings->window_size = window_size;

    settings->minl = minl;
    settings->maxl = maxl;
    settings->W = W;
    settings->H = H;


    settings->sax_upper_byte_size = settings->sax_byte_size;
    settings->sax_lower_byte_size = settings->sax_byte_size;
    settings->window_size_byte_size = sizeof(int);
    settings->start_pos_byte_size = sizeof(file_position_type) * window_size;
    settings->min_len_byte_size = sizeof(int) * window_size;
    // settings->ts_byte_size = (sizeof(ts_type) * timeseries_size);
    // settings->position_byte_size = sizeof(file_position_type);

    settings->full_record_size = settings->sax_byte_size + settings->sax_upper_byte_size + settings->sax_lower_byte_size + settings->window_size_byte_size + settings->start_pos_byte_size + settings->min_len_byte_size;

    // settings->full_record_size = settings->sax_byte_size
    // + settings->position_byte_size
    // + settings->ts_byte_size;

    //////////////

    settings->sax_bit_cardinality = sax_bit_cardinality;

    settings->max_sax_cardinalities = malloc(sizeof(sax_type) * paa_segments);

    for (int i = 0; i < settings->paa_segments; i++)
        settings->max_sax_cardinalities[i] = settings->sax_bit_cardinality;

    settings->sax_alphabet_cardinality = pow(2, sax_bit_cardinality);

    // settings->mindist_sqrt = sqrtf((float) settings->timeseries_size /
    //                                (float) settings->paa_segments);

    settings->mindist_sqrt = ((float)settings->timeseries_size /
                              (float)settings->paa_segments);

    settings->root_nodes_size = pow(2, settings->paa_segments);

    // SEGMENTS * (CARDINALITY)
    float c_size = ceil(log10(settings->sax_alphabet_cardinality + 1));
    // settings->max_filename_size = settings->paa_segments *
    //                               ((c_size * 2) + 2)
    //                               + 5 + strlen(root_directory);
    settings->max_filename_size = settings->paa_segments *
                                      ((c_size * 2) + 2) +
                                  10 + strlen(root_directory);

    if (paa_segments > sax_bit_cardinality)
    {
        settings->bit_masks = malloc(sizeof(root_mask_type) * (paa_segments + 1));
        if (settings->bit_masks == NULL)
        {
            fprintf(stderr, "error: could not allocate memory for bit masks.\n");
            return NULL;
        }

        for (; paa_segments >= 0; paa_segments--)
        {
            settings->bit_masks[paa_segments] = pow(2, paa_segments);
        }
    }
    else
    {
        settings->bit_masks = malloc(sizeof(root_mask_type) * (sax_bit_cardinality + 1));
        if (settings->bit_masks == NULL)
        {
            fprintf(stderr, "error: could not allocate memory for bit masks.\n");
            return NULL;
        }

        for (; sax_bit_cardinality >= 0; sax_bit_cardinality--)
        {
            settings->bit_masks[sax_bit_cardinality] = pow(2, sax_bit_cardinality);
        }
    }

    return settings;
}

/**
 This function initializes an isax index
 @param isax_index_settings *settings
 @return isax_index
 */
isax_index *isax_index_init(isax_index_settings *settings)
{
    // index->total_bytes = 0;

    isax_index *index = malloc(sizeof(isax_index));
    // index->total_bytes = 0;

    if (index == NULL)
    {
        fprintf(stderr, "error: could not allocate memory for index structure.\n");
        return NULL;
    }
    index->settings = settings;
    index->first_node = NULL;
    index->fbl = initialize_fbl(settings->initial_fbl_buffer_size,
                                pow(2, settings->paa_segments),
                                settings->max_total_buffer_size, index);
    index->total_records = 0;
    index->root_nodes = 0;

    init_stats(index);

    return index;
}

/**
 This function destroys an index.
 @param isax_index *index
 @param isax_ndoe *node
 */
void isax_index_destroy(isax_index *index, int use_index, isax_node *node)
{
    if (node == NULL)
    {

        // TODO: OPTIMIZE TO FLUSH WITHOUT TRAVERSAL!
        isax_node *subtree_root = index->first_node;
        while (subtree_root != NULL)
        {
            isax_node *next = subtree_root->next;
            isax_index_destroy(index, use_index, subtree_root);
            subtree_root = next;
        }
        destroy_fbl(index->fbl, use_index);
    }
    else
    {
        // Traverse tree
        if (!node->is_leaf)
        {
            isax_index_destroy(index, use_index, node->right_child);
            isax_index_destroy(index, use_index, node->left_child);
        }

        if (node->split_data != NULL)
        {
            free(node->split_data->split_mask);
            free(node->split_data);
        }
        if (node->filename != NULL)
        {
            free(node->filename);
        }
        if (node->isax_cardinalities != NULL)
        {
            free(node->isax_cardinalities);
        }
        if (node->isax_values != NULL)
        {
            free(node->isax_values);
        }
        if (node->sax_upper_max_values != NULL)
        {
            free(node->sax_upper_max_values);
        }
        if (node->sax_lower_max_values != NULL)
        {
            free(node->sax_lower_max_values);
        }
        if (node->isax_upper_values != NULL)
        {
            free(node->isax_upper_values);
        }
        if (node->isax_lower_values != NULL)
        {
            free(node->isax_lower_values);
        }

        // free_isax_upper;

        free(node);
    }
}

/*
float calculate_node_distance (isax_index *index, isax_node *node, ts_type *query,
                               file_position_type *bsf_position) {
    COUNT_CHECKED_NODE()
    float bsf = MAXFLOAT;
    *bsf_position = -1;

    if (node->has_full_data_file) {
        char * full_fname = malloc(sizeof(char) * (strlen(node->filename) + 4));
        strcpy(full_fname, node->filename);
        strcat(full_fname, ".ts");

        COUNT_INPUT_TIME_START
        FILE * full_file = fopen(full_fname, "r");
        COUNT_INPUT_TIME_END

        if(full_file != NULL) {
            COUNT_INPUT_TIME_START
            fseek(full_file, 0L, SEEK_END);
            size_t file_size = ftell(full_file);
            fseek(full_file, 0L, SEEK_SET);
            COUNT_INPUT_TIME_END
            int file_records = (int) file_size / (int)(index->settings->ts_byte_size + sizeof(file_position_type));
            ts_type *ts = malloc(index->settings->ts_byte_size);
            file_position_type *position =  malloc(sizeof(file_position_type));
            while (file_records > 0) {
                //printf("POS: %d\n", ftell(full_file));
        COUNT_INPUT_TIME_START
                fread(ts, sizeof(ts_type), index->settings->timeseries_size, full_file);
                fread(position, sizeof(file_position_type), 1, full_file);
        COUNT_INPUT_TIME_END
                float dist = ts_euclidean_distance(query, ts,
                                                   index->settings->timeseries_size);
                if (dist < bsf) {
                    bsf = dist;
                    *bsf_position = *position;
                }
                file_records--;
            }

            free(ts);
            free(position);
        }

        fclose(full_file);
        free(full_fname);
    }
    else {
        printf("**** NO FULL DATA FILE...\n");
        exit(FAILURE);
    }

    return bsf;
}
*/

// int queue_bounded_sorted_insert(struct  query_result *q, struct query_result d, unsigned int *cur_size, unsigned int k)
// {
//     struct query_result  temp;
//     size_t i;
//     size_t newsize;

//    bool is_duplicate = false;
//    for (unsigned int itr = 0 ; itr < *cur_size ; ++itr)
//    {
//      if (q[itr].distance == d.distance)
//        is_duplicate = true;
//    }

//    if (!is_duplicate)
//      {
//        /* the queue is full, overwrite last element*/
//        if (*cur_size == k) {
// 	 q[k-1].distance = d.distance;
// 	 q[k-1].node = d.node;
// 	 q[k-1].raw_file_position = d.raw_file_position;
//        }
//        else
// 	 {
// 	   q[*cur_size].distance = d.distance;
// 	   q[*cur_size].node = d.node;
// 	   q[*cur_size].raw_file_position = d.raw_file_position;
// 	   ++(*cur_size);
// 	 }

//        unsigned int idx,j;

//        idx = 1;

//        while (idx < *cur_size)
// 	 {
// 	   j = idx;
// 	   while ( j > 0 &&  ( (q[j-1]).distance > q[j].distance))
// 	     {
// 	       temp = q[j];
// 	       q[j].distance = q[j-1].distance;
// 	       q[j].node = q[j-1].node;
// 	       q[j].raw_file_position = q[j-1].raw_file_position;
// 	       q[j-1].distance = temp.distance;
// 	       q[j-1].node = temp.node;
// 	       q[j-1].raw_file_position = temp.raw_file_position;
// 	       --j;
// 	     }
// 	   ++idx;
// 	 }
//      }
//    return 0;
// }

// void calculate_node_knn_distance (isax_index *index, isax_node *node,
// 				  ts_type *query_ts_reordered, int *query_order,
// 				  unsigned int offset, ts_type bsf,
// 				  unsigned int k,
// 				  struct query_result  *knn_results,
// 				  unsigned int *cur_size)

// {

//     //float bsf = MAXFLOAT;
//     //*bsf_position = -1;

//   ts_type kth_bsf = FLT_MAX;

//   if (node->has_full_data_file) {
//     char * full_fname = malloc(sizeof(char) * (strlen(node->filename) + 4));
//     strcpy(full_fname, node->filename);
//     strcat(full_fname, ".ts");

//     COUNT_PARTIAL_RAND_INPUT
//       COUNT_PARTIAL_INPUT_TIME_START
//       FILE * full_file = fopen(full_fname, "r");
//     COUNT_PARTIAL_INPUT_TIME_END

//       if(full_file != NULL) {
// 	COUNT_PARTIAL_INPUT_TIME_START
// 	  COUNT_PARTIAL_RAND_INPUT
// 	  fseek(full_file, 0L, SEEK_END);
// 	size_t file_size = ftell(full_file);
// 	COUNT_PARTIAL_RAND_INPUT
// 	  fseek(full_file, 0L, SEEK_SET);
// 	COUNT_PARTIAL_INPUT_TIME_END
// 	  int file_records = (int) file_size / (int)(index->settings->ts_byte_size + sizeof(file_position_type));
// 	ts_type *ts = malloc(index->settings->ts_byte_size);
// 	file_position_type *position =  malloc(sizeof(file_position_type));

// 	COUNT_CHECKED_NODE
// 	  COUNT_CHECKED_TS(file_records)

// 	  COUNT_LOADED_NODE
// 	  COUNT_LOADED_TS(file_records)

// 	  while (file_records > 0) {
// 	    COUNT_PARTIAL_INPUT_TIME_START
// 	      COUNT_PARTIAL_SEQ_INPUT
// 	      fread(ts, sizeof(ts_type), index->settings->timeseries_size, full_file);
// 	    COUNT_PARTIAL_SEQ_INPUT
// 	      fread(position, sizeof(file_position_type), 1, full_file);
// 	    COUNT_PARTIAL_INPUT_TIME_END
// 	      struct query_result result;
// 	    result =  knn_results[k-1];

// 	    kth_bsf = result.distance;

// 	      ts_type dist = ts_euclidean_distance_reordered(query_ts_reordered,
// 							     ts,
// 							     offset,  //offset is 0 for whole matching
// 							     index->settings->timeseries_size,
// 							     kth_bsf,
// 							     query_order);

// 	    //float dist = ts_euclidean_distance(query, ts,
// 	    //                                   index->settings->timeseries_size);

// 	    if (dist < kth_bsf) {
// 	      struct query_result object_result;// =  malloc(sizeof(struct query_result));
// 	      object_result.node = node;
// 	      object_result.distance =  dist;
// 	      object_result.raw_file_position = position;
// 	      queue_bounded_sorted_insert(knn_results, object_result, cur_size, k);
// 	    }
// 	    file_records--;
// 	  }

// 	free(ts);
// 	free(position);
//       }
//     COUNT_PARTIAL_INPUT_TIME_START
//       fclose(full_file);
//     COUNT_PARTIAL_INPUT_TIME_END

//       free(full_fname);
//   }
//   else {
//     printf("**** NO FULL DATA FILE...\n");
//     exit(FAILURE);
//   }

// }

// ts_type calculate_node_distance (isax_index *index, isax_node *node,
// 				 ts_type *query_ts_reordered, int *query_order,
// 				 unsigned int offset, ts_type bsf,

// 				 file_position_type *bsf_position)
// {

//     //float bsf = MAXFLOAT;
//     //*bsf_position = -1;

//     if (node->has_full_data_file) {
//         char * full_fname = malloc(sizeof(char) * (strlen(node->filename) + 4));
//         strcpy(full_fname, node->filename);
//         strcat(full_fname, ".ts");

// 	COUNT_PARTIAL_RAND_INPUT
// 	COUNT_PARTIAL_INPUT_TIME_START
//         FILE * full_file = fopen(full_fname, "r");
//         COUNT_PARTIAL_INPUT_TIME_END

//         if(full_file != NULL) {
//             COUNT_PARTIAL_INPUT_TIME_START
// 	    COUNT_PARTIAL_RAND_INPUT
//             fseek(full_file, 0L, SEEK_END);
//             size_t file_size = ftell(full_file);
// 	    COUNT_PARTIAL_RAND_INPUT
//             fseek(full_file, 0L, SEEK_SET);
//             COUNT_PARTIAL_INPUT_TIME_END
//             int file_records = (int) file_size / (int)(index->settings->ts_byte_size + sizeof(file_position_type));
//             ts_type *ts = malloc(index->settings->ts_byte_size);
//             file_position_type *position =  malloc(sizeof(file_position_type));

//             COUNT_CHECKED_NODE
//             COUNT_CHECKED_TS(file_records)

//             COUNT_LOADED_NODE
//             COUNT_LOADED_TS(file_records)

//             while (file_records > 0) {
//                 COUNT_PARTIAL_INPUT_TIME_START
// 	        COUNT_PARTIAL_SEQ_INPUT
//                 fread(ts, sizeof(ts_type), index->settings->timeseries_size, full_file);
// 	        COUNT_PARTIAL_SEQ_INPUT
//                 fread(position, sizeof(file_position_type), 1, full_file);
//                 COUNT_PARTIAL_INPUT_TIME_END
//                 ts_type dist = ts_euclidean_distance_reordered(query_ts_reordered,
// 						ts,
// 						offset,  //offset is 0 for whole matching
// 						index->settings->timeseries_size,
// 						bsf,
// 						query_order);

//                 //float dist = ts_euclidean_distance(query, ts,
//                 //                                   index->settings->timeseries_size);

//                 if (dist < bsf) {
//                     bsf = dist;
//                     *bsf_position = *position;
//                 }
//                 file_records--;
//             }

//             free(ts);
//             free(position);
//         }
//         COUNT_PARTIAL_INPUT_TIME_START
//         fclose(full_file);
//         COUNT_PARTIAL_INPUT_TIME_END

//         free(full_fname);
//     }
//     else {
//         printf("**** NO FULL DATA FILE...\n");
//         exit(FAILURE);
//     }

//     return bsf;
// }

void isax_index_finalize(isax_index *index, isax_node *node, int *already_finalized)
{
    // exit(1);
    if (node == NULL)
    {
        isax_node *node = index->first_node;
        if (index->fbl->current_record_index != 0)
        {
            // printf("Flushing fbl from finalize-start...\n");
            flush_fbl(index->fbl, index);
            // printf("test\n");
            free(index->fbl->hard_buffer);
            // printf("Flushing fbl from finalize-end...\n");
        }

        while (node != NULL)
        {
            isax_index_finalize(index, node, already_finalized);
            node = node->next;
        }

        flush_all_leaf_buffers(index, 1);        
    }
    else
    {
        if (!node->is_leaf)
        {

            if (node->leaf_size > 0)
            {
                root_mask_type mask = index->settings->bit_masks[index->settings->sax_bit_cardinality -
                                                                 node->split_data->split_mask[node->split_data->splitpoint] - 1];
                char *sfname = malloc(sizeof(char) * (strlen(node->filename) + 5));
                strcpy(sfname, node->filename);
                strcat(sfname, ".sax");

                COUNT_PARTIAL_RAND_INPUT
                COUNT_PARTIAL_INPUT_TIME_START
                FILE *sax_file = fopen(sfname, "r");
                COUNT_PARTIAL_INPUT_TIME_END

                char *tsfname = malloc(sizeof(char) * (strlen(node->filename) + 4));
                strcpy(tsfname, node->filename);
                strcat(tsfname, ".ts");

                COUNT_PARTIAL_RAND_INPUT
                COUNT_PARTIAL_INPUT_TIME_START
                FILE *ts_file = fopen(tsfname, "r");
                COUNT_PARTIAL_INPUT_TIME_END
                //                FILE *to_add = fopen("toadd.bin", "a+");

                // If it can't open exit;
                if (sax_file != NULL && ts_file != NULL)
                {
                    isax_node_record *rbuf = malloc(sizeof(isax_node_record) * (node->leaf_size + 1));
                    int rbuf_index = 0;

                    COUNT_PARTIAL_INPUT_TIME_START
                    while (!feof(ts_file))
                    {

                        rbuf[rbuf_index].sax = malloc(sizeof(sax_type) * index->settings->paa_segments);
                        rbuf[rbuf_index].sax_upper = malloc(index->settings->sax_upper_byte_size);
                        rbuf[rbuf_index].sax_lower = malloc(index->settings->sax_lower_byte_size);
                        // rbuf[rbuf_index].window_size = malloc(sizeof(sax_type) * index->settings->paa_segments);
                        rbuf[rbuf_index].start_pos = malloc(index->settings->start_pos_byte_size);
                        rbuf[rbuf_index].min_len = malloc(index->settings->min_len_byte_size);

                        // rbuf[rbuf_index].ts = malloc(sizeof(ts_type) * index->settings->timeseries_size);

                        // If it can't read continue.
                        COUNT_PARTIAL_SEQ_INPUT
                        COUNT_PARTIAL_SEQ_INPUT

                        // HRTODO: reconstruct the logic of loading node file
                        // printf("read node file when building index\n");
                        if (!read_node_files(sax_file, ts_file, &rbuf[rbuf_index], index))
                        {

                            free(rbuf[rbuf_index].sax);
                            free(rbuf[rbuf_index].sax_upper);
                            free(rbuf[rbuf_index].sax_lower);
                            free(rbuf[rbuf_index].start_pos);
                            free(rbuf[rbuf_index].min_len);
                            break;
                        }
                        else
                        {
                            rbuf_index++;
                        }
                    }
                    COUNT_PARTIAL_INPUT_TIME_END

                    while (rbuf_index > 0)
                    {
                        rbuf_index--;
                        isax_node_record *r = &rbuf[rbuf_index];
                        isax_node *destination = node;

                        //                        fwrite(r->sax, sizeof(sax_type), index->settings->paa_segments, to_add);

                        while (!destination->is_leaf)
                        {
                            mask = index->settings->bit_masks[index->settings->sax_bit_cardinality -
                                                              destination->split_data->split_mask[destination->split_data->splitpoint] - 1];

                            if (mask & r->sax[destination->split_data->splitpoint])
                            {
                                destination = destination->right_child;
                            }
                            else
                            {
                                destination = destination->left_child;
                            }

                            // HRTODO mark here
                            // update the sax_upper info maintained by lazy push-down strategy.
                            // other sax_upper info will be updated in add_to_node_buffer.
                            update_node_sax_lower_upper(destination, r->sax_lower, r->sax_upper, index->settings->paa_segments);
                        }

                        add_to_node_buffer(destination, r, index->settings->paa_segments,
                                           index->settings->timeseries_size,
                                           index->settings->initial_leaf_buffer_size);

                        destination->leaf_size--;

                        //                        char * pfname = malloc(sizeof(char) * (strlen(destination->filename) + 5));
                        //                        strcpy(pfname, destination->filename);
                        //                        strcat(pfname, ".pre");
                        //                        if(!remove(pfname)) {
                        //                            printf("\nNO PREVIOUS FILENAME AT %s (Address: %d)\n",
                        //                                   destination->filename, (int)destination);
                        //                            exit(-1);
                        //                        }
                        //                        free(pfname);

                        if (destination->filename == NULL)
                        {
                            create_node_filename(index, destination, r);
                            printf("NO FILENAME, would be: %s (Address: %d)\n",
                                   destination->filename, (int)destination);
                            exit(-1);
                        }

                        // Increase the value of the already finalized time series
                        (*already_finalized)++;
                        // If total buffer size is reached then flush.
                        if (*already_finalized % index->settings->max_total_buffer_size == 0)
                        {
                            flush_all_leaf_buffers(index, 1);
                        }
                    }

                    free(rbuf);
                }
                // fclose(to_add);
                COUNT_PARTIAL_INPUT_TIME_START
                fclose(ts_file);
                fclose(sax_file);
                COUNT_PARTIAL_INPUT_TIME_END
                remove(sfname);
                remove(tsfname);
                free(sfname);
                free(tsfname);
            }
            else
            {
                COUNT_LOADED_NODE;
            }

            isax_index_finalize(index, node->left_child, already_finalized);
            isax_index_finalize(index, node->right_child, already_finalized);
        }
    }
    // set the isax_upper with sax_upper_max info in the node.

    if (node == NULL)
    {
        set_isax_lower_upper(index->first_node, index);
    }
}

/**
 This function inserts an isax_node_record in an isax index with an fbl
 @param isax_index *index
 @param isax_node_record *record
 */
enum response isax_fbl_index_insert(isax_index *index, isax_node_record *record)
{
    // Create mask for the first bit of the sax representation

    // Step 1: Check if there is a root node that represents the
    //         current node's sax representation

    // TODO: Create INSERTION SHORT AND BINARY SEARCH METHODS.
    root_mask_type first_bit_mask = 0x00;
    CREATE_MASK(first_bit_mask, index, record->sax); // find the target node at top layer
    // printf("test\n");
    insert_to_fbl(index->fbl, record, first_bit_mask, index);
    // printf("test1\n");

    index->total_records++;
    if ((index->total_records % index->settings->max_total_buffer_size) == 0)
    {
        // printf("Flushing fbl-start...\n");
        flush_fbl(index->fbl, index);
        // printf("Flushing fbl-end...\n");
    }

    return SUCCESS;
}

/*
 This function inserts an isax_node_record in an isax index
 @param isax_index *index
 @param isax_node_record *record
 */
enum response isax_index_insert(isax_index *index, isax_node_record *record)
{
    // Create mask for the first bit of the sax representation

    // Step 1: Check if there is a root node that represents the
    //         current node's sax representation

    // TODO: Create INSERTION SHORT AND BINARY SEARCH METHODS.
    root_mask_type first_bit_mask = 0x00;
    CREATE_MASK(first_bit_mask, index, record->sax);

    isax_node *curr_node = index->first_node;
    isax_node *last_node = NULL;
    while (curr_node != NULL)
    {
        if (first_bit_mask == curr_node->mask)
        {
            break;
        }
        last_node = curr_node;
        curr_node = curr_node->next;
    }

    // No appropriate root node has been found for this record
    if (curr_node == NULL)
    {
#ifdef DEBUG
        // Create node
        printf("*** Creating new root node. ***\n\n");
#endif
        isax_node *new_node = isax_root_node_init(first_bit_mask, index->settings->paa_segments,
                                                  index->settings->timeseries_size);
        index->root_nodes++;

        new_node->is_leaf = 1;

        if (index->first_node == NULL)
        {
            index->first_node = new_node;
        }
        else
        {
            last_node->next = new_node;
            new_node->previous = last_node;
        }

        curr_node = new_node;
    }

    enum response r = add_record_to_node(index, curr_node, record, 1);
    index->total_records++;

    // Check if flush to disk is needed
    // IMPORTANT TODO: READ TOTAL_BUFFERED RECORDS!!! AND NOT TOTAL RECORDS!!!!
    if ((index->total_records % index->settings->max_total_buffer_size) == 0)
    {
        // Passing 0 to not free the input data, since the data is stored
        // in the large memory array that will be reused afterwards
        flush_all_leaf_buffers(index, 0);
    }

    return r;
}

enum response create_node_filename(isax_index *index,
                                   isax_node *node,
                                   isax_node_record *record)
{
    int i;

    node->filename = malloc(sizeof(char) * index->settings->max_filename_size);
    sprintf(node->filename, "%s", index->settings->root_directory);
    int l = (int)strlen(index->settings->root_directory);

    // If this has a parent then it is not a root node and as such it does have some
    // split data on its parent about the cardinalities.
    node->isax_values = malloc(sizeof(sax_type) * index->settings->paa_segments);
    node->isax_cardinalities = malloc(sizeof(sax_type) * index->settings->paa_segments);

    node->isax_lower_values = malloc(sizeof(sax_type) * index->settings->paa_segments);
    node->isax_upper_values = malloc(sizeof(sax_type) * index->settings->paa_segments);

    if (node->parent)
    {
        for (i = 0; i < index->settings->paa_segments; i++)
        {
            root_mask_type mask = 0x00;
            int k;
            for (k = 0; k <= node->parent->split_data->split_mask[i]; k++)
            {
                mask |= (index->settings->bit_masks[index->settings->sax_bit_cardinality - 1 - k] &
                         record->sax[i]);
            }
            mask = mask >> index->settings->sax_bit_cardinality - node->parent->split_data->split_mask[i] - 1;

            node->isax_values[i] = (int)mask;
            node->isax_cardinalities[i] = node->parent->split_data->split_mask[i] + 1;

            if (i == 0)
            {
                l += sprintf(node->filename + l, "%d.%d", node->isax_values[i], node->isax_cardinalities[i]);
            }
            else
            {
                l += sprintf(node->filename + l, "_%d.%d", node->isax_values[i], node->isax_cardinalities[i]);
            }
        }
    }
    // If it has no parent it is root node and as such it's cardinality is 1.
    else
    {
        root_mask_type mask = 0x00;

        for (i = 0; i < index->settings->paa_segments; i++)
        {

            mask = (index->settings->bit_masks[index->settings->sax_bit_cardinality - 1] & record->sax[i]);
            mask = mask >> index->settings->sax_bit_cardinality - 1;

            node->isax_values[i] = (int)mask;
            node->isax_cardinalities[i] = 1;

            if (i == 0)
            {
                l += sprintf(node->filename + l, "%d.1", (int)mask);
            }
            else
            {
                l += sprintf(node->filename + l, "_%d.1", (int)mask);
            }
        }
    }

#ifdef DEBUG
    printf("\tCreated filename:\t\t %s\n\n", node->filename);
#endif

    return SUCCESS;
}

enum response add_record_to_node(isax_index *index,
                                 isax_node *tree_node,
                                 isax_node_record *record,
                                 const char leaf_size_check)
{
    isax_node *node = tree_node;

    // Traverse tree
    while (!node->is_leaf)
    {
        int location = index->settings->sax_bit_cardinality - 1 -
                       node->split_data->split_mask[node->split_data->splitpoint];

        root_mask_type mask = index->settings->bit_masks[location];

        if (record->sax[node->split_data->splitpoint] & mask)
        {
            node = node->right_child;
        }
        else
        {
            node = node->left_child;
        }
    }

    // Check if split needed
    if ((node->leaf_size) >= index->settings->max_leaf_size && leaf_size_check)
    {
#ifdef DEBUG
        printf(">>> %s leaf size: %d\n\n", node->filename, node->leaf_size);
#endif
        split_node(index, node);
        add_record_to_node(index, node, record, leaf_size_check);
    }
    else
    {

        // printf("Adding node to buffer--start..\n");
        add_to_node_buffer(node, record, index->settings->paa_segments,
                           index->settings->timeseries_size,
                           index->settings->initial_leaf_buffer_size);
        // printf("Adding node to buffer--end..\n");
    }
    if ((node->sax_buffer_size == 1 || node->prev_sax_buffer_size == 1) && node->filename == NULL)
    {
        create_node_filename(index, node, record);
    }

    // index->total_buffered_records++;

    return SUCCESS;
}

/**
 Flushes all leaf buffers of a tree to disk.
 */
enum response flush_all_leaf_buffers(isax_index *index, const char clear_input_data)
{
#ifndef DEBUG
#if VERBOSE_LEVEL == 2
    printf("\n");
    fflush(stdout);
#endif
#else
    printf("*** FLUSHING ***\n\n");
#endif
    // TODO: OPTIMIZE TO FLUSH WITHOUT TRAVERSAL!
    isax_node *subtree_root = index->first_node;

#ifndef DEBUG
#if VERBOSE_LEVEL == 2
    int i = 1;
    fprintf(stdout, "\x1b[31mFlushing: \x1b[36m00.00%%\x1b[0m");
#endif
#if VERBOSE_LEVEL == 1
    printf("Flushing...\n");
#endif
#endif

    while (subtree_root != NULL)
    {
#ifndef DEBUG
#if VERBOSE_LEVEL == 2
        fprintf(stdout, "\r\x1b[31mFlushing: \x1b[36m%2.2lf%%\x1b[0m", ((float)i / (float)index->root_nodes) * 100);
        i++;
        fflush(stdout);
#endif
#endif
        if (flush_subtree_leaf_buffers(index, subtree_root) == FAILURE)
            return FAILURE;
        subtree_root = subtree_root->next;
    }
#ifndef DEBUG
#if VERBOSE_LEVEL == 2
    printf("\n");
#endif
#endif

    // *** FREE MEMORY OF NODES BUFFERS ***
    // Now that we have flushed its contents
    // there is no need to keep memory allocated.
    isax_index_clear_node_buffers(index, NULL, 1, clear_input_data);

    return SUCCESS;
}

/**
 Flushes the leaf buffers of a sub-tree to disk.
 */
enum response flush_subtree_leaf_buffers(isax_index *index, isax_node *node)
{
    // Set that the node has flushed data in the disk
    if (node->sax_buffer_size > 0 ||
        node->prev_sax_buffer_size > 0)
    {
        node->has_full_data_file = 1;
    }

    flush_node_buffer(node, index->settings);

    if (node->left_child != NULL)
    {
        flush_subtree_leaf_buffers(index, node->left_child);
    }

    if (node->right_child != NULL)
    {
        flush_subtree_leaf_buffers(index, node->right_child);
    }

    return SUCCESS;
}

void isax_index_clear_node_buffers(isax_index *index, isax_node *node,
                                   const char clear_children,
                                   const char clear_input_data)
{
    if (node == NULL)
    {
        // TODO: OPTIMIZE TO FLUSH WITHOUT TRAVERSAL!
        isax_node *subtree_root = index->first_node;

        while (subtree_root != NULL)
        {
            isax_index_clear_node_buffers(index, subtree_root, clear_children, clear_input_data);
            subtree_root = subtree_root->next;
        }
    }
    else
    {
        // Traverse tree
        if (!node->is_leaf && clear_children)
        {
            isax_index_clear_node_buffers(index, node->right_child, clear_children, clear_input_data);
            isax_index_clear_node_buffers(index, node->left_child, clear_children, clear_input_data);
        }
        clear_node_buffer(node, clear_input_data);
    }
}

void node_write(isax_index *index, isax_node *node, FILE *file)
{

    COUNT_PARTIAL_OUTPUT_TIME_START
    COUNT_PARTIAL_SEQ_OUTPUT
    COUNT_PARTIAL_SEQ_OUTPUT
    fwrite(&(node->is_leaf), sizeof(unsigned char), 1, file);
    fwrite(&(node->mask), sizeof(root_mask_type), 1, file);
    COUNT_PARTIAL_OUTPUT_TIME_END

    if (node->is_leaf)
    {
        if (node->filename != NULL)
        {
            int filename_size = strlen(node->filename);
            COUNT_PARTIAL_SEQ_OUTPUT
            COUNT_PARTIAL_SEQ_OUTPUT
            COUNT_PARTIAL_SEQ_OUTPUT
            COUNT_PARTIAL_SEQ_OUTPUT
            COUNT_PARTIAL_OUTPUT_TIME_START
            fwrite(&filename_size, sizeof(int), 1, file);
            fwrite(node->filename, sizeof(char), filename_size, file);
            fwrite(&(node->leaf_size), sizeof(int), 1, file);
            fwrite(&(node->level), sizeof(int), 1, file);
            fwrite(&(node->has_full_data_file), sizeof(char), 1, file);
            // fwrite(&(node->has_partial_data_file), sizeof(char), 1, file);
            COUNT_PARTIAL_OUTPUT_TIME_END
            COUNT_LEAF_NODE
            // collect stats while traversing the tree
            update_index_stats(index, node);
        }
        else
        {
            int filename_size = 0;
            COUNT_PARTIAL_SEQ_OUTPUT
            COUNT_PARTIAL_OUTPUT_TIME_START
            fwrite(&filename_size, sizeof(int), 1, file);
            COUNT_PARTIAL_OUTPUT_TIME_END
        }
    }
    else
    {
        COUNT_PARTIAL_SEQ_OUTPUT
        COUNT_PARTIAL_SEQ_OUTPUT
        COUNT_PARTIAL_OUTPUT_TIME_START
        fwrite(&(node->split_data->splitpoint), sizeof(int), 1, file);
        fwrite(node->split_data->split_mask, sizeof(sax_type), index->settings->paa_segments, file);
        COUNT_PARTIAL_OUTPUT_TIME_END
    }

    if (node->isax_cardinalities != NULL)
    {
        char has_isax_data = 1;
        COUNT_PARTIAL_SEQ_OUTPUT
        COUNT_PARTIAL_SEQ_OUTPUT
        COUNT_PARTIAL_SEQ_OUTPUT
        COUNT_PARTIAL_OUTPUT_TIME_START
        fwrite(&has_isax_data, sizeof(char), 1, file);
        fwrite(node->isax_cardinalities, sizeof(sax_type), index->settings->paa_segments, file);
        fwrite(node->isax_values, sizeof(sax_type), index->settings->paa_segments, file);
        fwrite(node->isax_lower_values, sizeof(sax_type), index->settings->paa_segments, file);
        fwrite(node->isax_upper_values, sizeof(sax_type), index->settings->paa_segments, file);
        COUNT_PARTIAL_OUTPUT_TIME_END
    }
    else
    {
        char has_isax_data = 0;
        COUNT_PARTIAL_SEQ_OUTPUT
        COUNT_PARTIAL_OUTPUT_TIME_START
        fwrite(&has_isax_data, sizeof(char), 1, file);
        COUNT_PARTIAL_OUTPUT_TIME_END
    }

    if (!node->is_leaf)
    {
        node_write(index, node->left_child, file);
        node_write(index, node->right_child, file);
    }
}

isax_node *node_read(isax_index *index, FILE *file)
{
    COUNT_NEW_NODE
    isax_node *node;
    unsigned char is_leaf = 0;
    root_mask_type mask = 0;

    COUNT_PARTIAL_SEQ_INPUT
    COUNT_PARTIAL_SEQ_INPUT
    COUNT_PARTIAL_INPUT_TIME_START

    fread(&is_leaf, sizeof(unsigned char), 1, file);
    fread(&(mask), sizeof(root_mask_type), 1, file);
    COUNT_PARTIAL_INPUT_TIME_END

    if (is_leaf)
    {
        node = malloc(sizeof(isax_node));
        node->is_leaf = 1;
        // node->has_partial_data_file = 0;
        node->has_full_data_file = 0;
        node->right_child = NULL;
        node->left_child = NULL;
        node->parent = NULL;
        node->next = NULL;
        node->leaf_size = 0;
        node->filename = NULL;
        node->isax_values = NULL;
        node->isax_cardinalities = NULL;
        node->previous = NULL;
        node->split_data = NULL;
        // node->buffer = init_node_buffer(index->settings->initial_leaf_buffer_size);
        node->mask = 0;

        node->sax_buffer_size = 0;
        node->prev_sax_buffer_size = 0;
        node->max_sax_buffer_size = 0;
        node->max_prev_sax_buffer_size = 0;

        node->env_buffer_size = 0;
        node->max_env_buffer_size = 0;

        node->level = 0;

        int filename_size = 0;
        COUNT_PARTIAL_INPUT_TIME_START
        COUNT_PARTIAL_SEQ_INPUT
        fread(&filename_size, sizeof(int), 1, file);
        COUNT_PARTIAL_INPUT_TIME_END
        if (filename_size > 0)
        {
            node->filename = malloc(sizeof(char) * (filename_size + 1));
            COUNT_PARTIAL_INPUT_TIME_START
            COUNT_PARTIAL_SEQ_INPUT
            COUNT_PARTIAL_SEQ_INPUT
            COUNT_PARTIAL_SEQ_INPUT
            COUNT_PARTIAL_SEQ_INPUT
            COUNT_PARTIAL_SEQ_INPUT
            fread(node->filename, sizeof(char), filename_size, file);
            fread(&(node->leaf_size), sizeof(int), 1, file);
            fread(&(node->level), sizeof(int), 1, file);
            // printf ("leaf_size = %d\n", node->leaf_size);
            fread(&(node->has_full_data_file), sizeof(char), 1, file);
            // fread(&(node->has_partial_data_file), sizeof(char), 1, file);
            COUNT_PARTIAL_INPUT_TIME_END
            node->filename[filename_size] = '\0';
            // COUNT_NEW_NODE
            COUNT_LEAF_NODE

            index->stats->leaves_heights[index->stats->leaves_counter] = node->level + 1;
            index->stats->leaves_sizes[index->stats->leaves_counter] = node->leaf_size;
            ++(index->stats->leaves_counter);

            update_index_stats(index, node);
            /*
                if(node->has_full_data_file) {
                    float number_of_bytes = (float) (node->leaf_size * index->settings->full_record_size);
                    int number_of_pages = ceil(number_of_bytes / (float) PAGE_SIZE);
                    //index->memory_info.disk_data_full += number_of_pages;
                }

                else if(node->has_partial_data_file) {
                    float number_of_bytes = (float) (node->leaf_size * index->settings->partial_record_size);
                    int number_of_pages = ceil(number_of_bytes / (float) PAGE_SIZE);
                    index->memory_info.disk_data_partial += number_of_pages;
                }
                if(node->has_full_data_file && node->has_partial_data_file) {
                    printf("WARNING: (Mem size counting) this leaf has both partial and full data.\n");
            }*/
        }
        else
        {
            node->filename = NULL;
            node->leaf_size = 0;
            node->has_full_data_file = 0;
            // node->has_partial_data_file = 0;
        }
    }
    else
    {
        node = malloc(sizeof(isax_node));
        // node->buffer = NULL;
        node->is_leaf = 0;
        node->filename = NULL;
        node->has_full_data_file = 0;
        // node->has_partial_data_file = 0;
        node->leaf_size = 0;
        node->split_data = malloc(sizeof(isax_node_split_data));
        node->split_data->split_mask = malloc(sizeof(sax_type) * index->settings->paa_segments);
        COUNT_PARTIAL_SEQ_INPUT
        COUNT_PARTIAL_SEQ_INPUT
        COUNT_PARTIAL_INPUT_TIME_START
        fread(&(node->split_data->splitpoint), sizeof(int), 1, file);
        fread(node->split_data->split_mask, sizeof(sax_type), index->settings->paa_segments, file);
        COUNT_PARTIAL_INPUT_TIME_END
    }
    node->mask = mask;

    char has_isax_data = 0;
    COUNT_PARTIAL_SEQ_INPUT
    COUNT_PARTIAL_INPUT_TIME_START
    fread(&has_isax_data, sizeof(char), 1, file);
    COUNT_PARTIAL_INPUT_TIME_END

    if (has_isax_data)
    {
        node->isax_cardinalities = malloc(sizeof(sax_type) * index->settings->paa_segments);
        node->isax_values = malloc(sizeof(sax_type) * index->settings->paa_segments);
        node->isax_lower_values = malloc(sizeof(sax_type) * index->settings->paa_segments);
        node->isax_upper_values = malloc(sizeof(sax_type) * index->settings->paa_segments);
        COUNT_PARTIAL_SEQ_INPUT
        COUNT_PARTIAL_INPUT_TIME_START
        fread(node->isax_cardinalities, sizeof(sax_type), index->settings->paa_segments, file);
        fread(node->isax_values, sizeof(sax_type), index->settings->paa_segments, file);
        fread(node->isax_lower_values, sizeof(sax_type), index->settings->paa_segments, file);
        fread(node->isax_upper_values, sizeof(sax_type), index->settings->paa_segments, file);
        COUNT_PARTIAL_INPUT_TIME_END
    }
    else
    {
        node->isax_cardinalities = NULL;
        node->isax_values = NULL;
    }

    if (!is_leaf)
    {
        node->left_child = node_read(index, file);
        node->right_child = node_read(index, file);
    }

    return node;
}

void index_write(isax_index *index)
{
    // fprintf(stderr, ">>> Storing index: %s\n", index->settings->root_directory);
    const char *filename = malloc(sizeof(char) * (strlen(index->settings->root_directory) + 15));
    filename = strcpy(filename, index->settings->root_directory);
    filename = strcat(filename, "index.idx\0");
    COUNT_PARTIAL_RAND_OUTPUT
    COUNT_PARTIAL_OUTPUT_TIME_START
    FILE *file = fopen(filename, "wb");
    if (file == NULL)
    {
        printf("Can not open %s.\n", filename);
        exit(1);
    }
    COUNT_PARTIAL_OUTPUT_TIME_END
    free(filename);

    // int raw_filename_size = strlen(index->settings->raw_filename);
    int timeseries_size = index->settings->timeseries_size;
    int paa_segments = index->settings->paa_segments;
    int sax_bit_cardinality = index->settings->sax_bit_cardinality;
    int max_leaf_size = index->settings->max_leaf_size;
    // int min_leaf_size = index->settings->min_leaf_size;
    int initial_leaf_buffer_size = index->settings->initial_leaf_buffer_size;
    unsigned long long max_total_buffer_size = index->settings->max_total_buffer_size;
    int initial_fbl_buffer_size = index->settings->initial_fbl_buffer_size;
    int window_size = index->settings->window_size;
    int minl = index->settings->minl;
    int maxl = index->settings->maxl;
    int W = index->settings->W;
    int H = index->settings->H;
    // int total_loaded_leaves = index->settings->total_loaded_leaves;
    // int tight_bound = index->settings->tight_bound;
    // int aggressive_check = index->settings->aggressive_check;
    int new_index = 0;

    // SETTINGS DATA
    COUNT_PARTIAL_SEQ_OUTPUT
    COUNT_PARTIAL_SEQ_OUTPUT
    COUNT_PARTIAL_SEQ_OUTPUT
    COUNT_PARTIAL_SEQ_OUTPUT
    COUNT_PARTIAL_SEQ_OUTPUT
    COUNT_PARTIAL_SEQ_OUTPUT
    COUNT_PARTIAL_SEQ_OUTPUT

    COUNT_PARTIAL_OUTPUT_TIME_START
    // fwrite(&raw_filename_size, sizeof(int), 1, file);
    // fwrite(index->settings->raw_filename, sizeof(char), raw_filename_size, file);

    // write the number of leaves, for now it is zero, then reset to the correct value
    // after all nodes are written, leaf_nodes_count is a global variable set in globals.h
    fwrite(&leaf_nodes_count, sizeof(unsigned long), 1, file);
    fwrite(&timeseries_size, sizeof(int), 1, file);
    fwrite(&paa_segments, sizeof(int), 1, file);
    fwrite(&sax_bit_cardinality, sizeof(int), 1, file);
    fwrite(&max_leaf_size, sizeof(int), 1, file);
    // fwrite(&min_leaf_size, sizeof(int), 1, file);
    fwrite(&initial_leaf_buffer_size, sizeof(int), 1, file);
    fwrite(&max_total_buffer_size, sizeof(unsigned long long), 1, file);
    fwrite(&initial_fbl_buffer_size, sizeof(int), 1, file);
    fwrite(&window_size, sizeof(int), 1, file);

    fwrite(&minl, sizeof(int), 1, file);
    fwrite(&maxl, sizeof(int), 1, file);
    fwrite(&W, sizeof(int), 1, file);
    fwrite(&H, sizeof(int), 1, file);


    // fwrite(&total_loaded_leaves, sizeof(int), 1, file);
    // fwrite(&tight_bound, sizeof(int), 1, file);
    // fwrite(&aggressive_check, sizeof(int), 1, file);
    COUNT_PARTIAL_OUTPUT_TIME_END

    // FBL DATA AND NODES
    int j;
    for (j = 0; j < index->fbl->number_of_buffers; j++)
    {
        first_buffer_layer_node *current_fbl_node = &index->fbl->buffers[j];
        if (current_fbl_node->initialized && current_fbl_node->node != NULL)
        {
            COUNT_PARTIAL_OUTPUT_TIME_START
            COUNT_PARTIAL_SEQ_OUTPUT
            fwrite(&j, sizeof(int), 1, file);
            COUNT_PARTIAL_OUTPUT_TIME_END
            node_write(index, current_fbl_node->node, file);
        }
    }
    COUNT_PARTIAL_OUTPUT_TIME_START

    // seek to the beg of the file to write the correct
    // number of leaves
    fseek(file, 0L, SEEK_SET);
    fwrite(&leaf_nodes_count, sizeof(unsigned long), 1, file);
    fclose(file);
    COUNT_PARTIAL_OUTPUT_TIME_END
    // const char *filename_adpt = malloc(sizeof(char) * (strlen(index->settings->root_directory) + 15));
    // filename_adpt = strcpy(filename_adpt, index->settings->root_directory);
    // filename_adpt = strcat(filename_adpt, "/adaptive");
    // COUNT_PARTIAL_RAND_OUTPUT
    // COUNT_PARTIAL_OUTPUT_TIME_START
    // FILE *file_adpt = fopen(filename_adpt, "wb");
    //  COUNT_PARTIAL_RAND_OUTPUT
    // COUNT_PARTIAL_RAND_OUTPUT
    // fwrite(&(index->total_records), sizeof(unsigned long long), 1, file_adpt);
    // fwrite(&(index->loaded_records), sizeof(unsigned long long), 1, file_adpt);
    // fclose(file_adpt);
    // COUNT_PARTIAL_OUTPUT_TIME_END
    // free(filename_adpt);
}

isax_index *index_read(const char *root_directory)
{
    // fprintf(stderr, ">>> Loading index: %s\n", root_directory);
    const char *filename = malloc(sizeof(char) * (strlen(root_directory) + 15));
    filename = strcpy(filename, root_directory);
    filename = strcat(filename, "index.idx\0");
    COUNT_PARTIAL_RAND_INPUT
    COUNT_PARTIAL_INPUT_TIME_START
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        printf("can not open file %s.\n", filename);
        exit(1);
    }
    COUNT_PARTIAL_INPUT_TIME_END
    free(filename);

    // int raw_filename_size = 0;
    // char *raw_filename = NULL;
    unsigned long count_leaves = 0;
    int timeseries_size = 0;
    int paa_segments = 0;
    int sax_bit_cardinality = 0;
    int max_leaf_size = 0;
    // int min_leaf_size = 0;
    int initial_leaf_buffer_size = 0;
    unsigned long long max_total_buffer_size = 0;
    int initial_fbl_buffer_size = 0;
    int window_size = 10;

    int minl, maxl, W, H;
    // int total_loaded_leaves = 0;
    // int tight_bound = 0;
    // int aggressive_check = 0;
    int new_index = 0;

    COUNT_PARTIAL_SEQ_INPUT
    COUNT_PARTIAL_INPUT_TIME_START
    // fread(&raw_filename_size, sizeof(int), 1, file);
    COUNT_PARTIAL_INPUT_TIME_END
    // raw_filename = malloc(sizeof(char) * (raw_filename_size+1));
    COUNT_PARTIAL_SEQ_INPUT
    COUNT_PARTIAL_INPUT_TIME_START
    // fread(raw_filename, sizeof(char), raw_filename_size, file);
    COUNT_PARTIAL_INPUT_TIME_END
    // raw_filename[raw_filename_size] = '\0';

    COUNT_PARTIAL_SEQ_OUTPUT
    COUNT_PARTIAL_SEQ_OUTPUT
    COUNT_PARTIAL_SEQ_OUTPUT
    COUNT_PARTIAL_SEQ_OUTPUT
    COUNT_PARTIAL_SEQ_OUTPUT
    COUNT_PARTIAL_SEQ_OUTPUT
    COUNT_PARTIAL_SEQ_OUTPUT
    COUNT_PARTIAL_SEQ_OUTPUT
    COUNT_PARTIAL_SEQ_OUTPUT
    COUNT_PARTIAL_SEQ_OUTPUT
    COUNT_PARTIAL_SEQ_OUTPUT

    COUNT_PARTIAL_INPUT_TIME_START
    fread(&count_leaves, sizeof(unsigned long), 1, file);
    fread(&timeseries_size, sizeof(int), 1, file);
    fread(&paa_segments, sizeof(int), 1, file);
    fread(&sax_bit_cardinality, sizeof(int), 1, file);
    fread(&max_leaf_size, sizeof(int), 1, file);
    // fread(&min_leaf_size, sizeof(int), 1, file);
    fread(&initial_leaf_buffer_size, sizeof(int), 1, file);
    fread(&max_total_buffer_size, sizeof(unsigned long long), 1, file);
    fread(&initial_fbl_buffer_size, sizeof(int), 1, file);
    fread(&window_size, sizeof(int), 1, file);

    fread(&minl, sizeof(int), 1, file);
    fread(&maxl, sizeof(int), 1, file);
    fread(&W, sizeof(int), 1, file);
    fread(&H, sizeof(int), 1, file);

    // fread(&total_loaded_leaves, sizeof(int), 1, file);
    // fread(&tight_bound, sizeof(int), 1, file);
    // fread(&aggressive_check, sizeof(int), 1, file);
    COUNT_PARTIAL_INPUT_TIME_END

    isax_index_settings *idx_settings = isax_index_settings_init(root_directory,
                                                                 timeseries_size,
                                                                 paa_segments,
                                                                 sax_bit_cardinality,
                                                                 max_leaf_size,
                                                                 initial_leaf_buffer_size,
                                                                 max_total_buffer_size,
                                                                 initial_fbl_buffer_size,
                                                                 0, window_size,
                                                                 minl, maxl, W, H);
    // idx_settings->raw_filename = malloc(sizeof(char) * 256);
    // strcpy(idx_settings->raw_filename, raw_filename);
    // free(raw_filename);

    isax_index *index = isax_index_init(idx_settings);

    index->stats->leaves_heights = calloc(count_leaves, sizeof(int));
    index->stats->leaves_sizes = calloc(count_leaves, sizeof(int));
    index->stats->leaves_counter = 0;

    while (!feof(file))
    {
        int j = 0;
        COUNT_PARTIAL_SEQ_INPUT
        COUNT_PARTIAL_INPUT_TIME_START
        size_t bytes_read = fread(&j, sizeof(int), 1, file);
        COUNT_PARTIAL_INPUT_TIME_END
        if (bytes_read)
        {
            first_buffer_layer_node *current_buffer = &index->fbl->buffers[j];
            current_buffer->initialized = 1;
            current_buffer->node = node_read(index, file);

            if (index->first_node == NULL)
            {
                index->first_node = current_buffer->node;
                current_buffer->node->next = NULL;
                current_buffer->node->previous = NULL;
            }
            else
            {
                isax_node *prev_first = index->first_node;
                index->first_node = current_buffer->node;
                index->first_node->next = prev_first;
                prev_first->previous = current_buffer->node;
            }
        }
    }

    COUNT_PARTIAL_INPUT_TIME_START
    fclose(file);
    COUNT_PARTIAL_INPUT_TIME_END

    /*
   const char *filename_adpt = malloc(sizeof(char) * (strlen(index->settings->root_directory) + 15));
   filename_adpt = strcpy(filename_adpt, index->settings->root_directory);
   filename_adpt = strcat(filename_adpt, "/adaptive");
   COUNT_PARTIAL_SEQ_INPUT
   COUNT_PARTIAL_INPUT_TIME_START
   FILE *file_adpt = fopen(filename_adpt, "rb");
   COUNT_PARTIAL_INPUT_TIME_END
   free(filename_adpt);

   if(file_adpt) {
       COUNT_PARTIAL_SEQ_INPUT
       COUNT_PARTIAL_SEQ_INPUT
       COUNT_PARTIAL_INPUT_TIME_START
       fread(&index->total_records, sizeof(unsigned long long), 1, file_adpt);
       fread(&index->loaded_records, sizeof(unsigned long long), 1, file_adpt);
       fclose(file_adpt);
       COUNT_PARTIAL_INPUT_TIME_END
   }
   */
    return index;
}

void isax_index_collect_stats(isax_index *index, isax_node *node)
{
    if (node == NULL)
    {

        isax_node *subtree_root = index->first_node;

        while (subtree_root != NULL)
        {
            // printf("new subtree with leaf_size = %d\n", subtree_root->leaf_size);
            isax_index_collect_subtree_stats(index, subtree_root);
            subtree_root = subtree_root->next;
        }
        // update_index_stats(index,subtree_root);
    }
}

void isax_index_collect_subtree_stats(isax_index *index, isax_node *node)
{
    if (node->right_child != NULL)
        isax_index_collect_subtree_stats(index, node->right_child);
    if (node->left_child != NULL)
        isax_index_collect_subtree_stats(index, node->left_child);
    if (node->right_child == NULL && node->left_child == NULL)
        update_index_stats(index, node);
}

/*
void update_index_stats(isax_index *index, isax_node *node)
{

  int height = node->level +1;
  int threshold = index->settings->max_leaf_size;
  int node_size = 0;

       if (node->is_leaf)
       {
         if (node->filename != NULL) {
            char * full_fname = malloc(sizeof(char) * (strlen(node->filename) + 4));
            strcpy(full_fname, node->filename);
            strcat(full_fname, ".ts");

            FILE * full_file = fopen(full_fname, "r");
            COUNT_PARTIAL_INPUT_TIME_END

            if(full_file != NULL) {
                fseek(full_file, 0L, SEEK_END);
                size_t file_size = ftell(full_file);
                fseek(full_file, 0L, SEEK_SET);
                int file_records = (int) file_size / (int)(index->settings->ts_byte_size + sizeof(file_position_type));
                node_size = node->leaf_size;
            //node_size = node->leaf_size;
            COUNT_LEAF_NODE

                    if (node->leaf_size > threshold || file_records > threshold)
                {
                printf("node %s has leaf_size %d and file records are %d\n", full_fname, node_size, file_records);
                }

                if(node_size == 0)
                {
                    COUNT_EMPTY_LEAF_NODE
                }

            COUNT_TOTAL_TS(node_size);

            double node_fill_factor = (node_size * 100.0)/threshold;

            if (node_fill_factor < index->stats->min_fill_factor)
            {
                    index->stats->min_fill_factor = node_fill_factor;
            }
            if (node_fill_factor > index->stats->max_fill_factor)
            {
                index->stats->max_fill_factor = node_fill_factor;
            }
            if (height < index->stats->min_height)
            {
                index->stats->min_height = height;
            }
            if (height > index->stats->max_height)
            {
                index->stats->max_height = height;
            }

            index->stats->sum_fill_factor += node_fill_factor ;
            index->stats->sum_squares_fill_factor += pow(node_fill_factor,2) ;

            index->stats->sum_height += height;
            index->stats->sum_squares_height += pow(height,2) ;

                }
                fclose(full_file);
            free(full_fname);

         }
     else
     {
                COUNT_EMPTY_LEAF_NODE
     }
    }
        else
        {
                COUNT_EMPTY_LEAF_NODE
        }
}
*/
void update_index_stats(isax_index *index, isax_node *node)
{

    int height = node->level + 1;
    int threshold = index->settings->max_leaf_size;
    int node_size = node->leaf_size;

    double node_fill_factor = (node_size * 100.0) / threshold;

    if (node_fill_factor < index->stats->min_fill_factor)
    {
        index->stats->min_fill_factor = node_fill_factor;
    }
    if (node_fill_factor > index->stats->max_fill_factor)
    {
        index->stats->max_fill_factor = node_fill_factor;
    }
    if (height < index->stats->min_height)
    {
        index->stats->min_height = height;
    }
    if (height > index->stats->max_height)
    {
        index->stats->max_height = height;
    }

    if (node_size == 0)
    {
        COUNT_EMPTY_LEAF_NODE
    }

    index->stats->sum_fill_factor += node_fill_factor;
    index->stats->sum_squares_fill_factor += pow(node_fill_factor, 2);

    index->stats->sum_height += height;
    index->stats->sum_squares_height += pow(height, 2);

    COUNT_TOTAL_TS(node_size)
}

// void get_queries_stats(isax_index * index, unsigned int queries_count)
// {

//   //total_non_zero_queries_count can be different from total_queries_count
//   //since the former only includes queries with non zero distance so
//   //we can calculate the effective epsilon

//   if (queries_count != 0)
//   {
//     index->stats->queries_avg_pruning_ratio =  ((double) index->stats->queries_sum_pruning_ratio)
//                                                / queries_count ;
//     index->stats->queries_sum_squares_pruning_ratio -= (pow(index->stats->queries_sum_pruning_ratio,2)
// 							/queries_count);
//     index->stats->queries_sd_pruning_ratio =  sqrt(((double) index->stats->queries_sum_squares_pruning_ratio)
// 						   / queries_count);
//     index->stats->queries_avg_checked_nodes_count /= queries_count;
//     index->stats->queries_avg_checked_ts_count /= queries_count;
//     index->stats->queries_avg_loaded_nodes_count /= queries_count;
//     index->stats->queries_avg_loaded_ts_count /= queries_count;

//     index->stats->queries_avg_tlb =  ((double) index->stats->queries_sum_tlb)
//                                                / queries_count  ;
//     index->stats->queries_sum_squares_tlb -= (pow(index->stats->queries_sum_tlb,2)
// 							/queries_count );
//     index->stats->queries_sd_tlb =  sqrt(((double) index->stats->queries_sum_squares_tlb)
// 						   / queries_count);
//   }

//   if ((queries_count - index->stats->eff_epsilon_queries_count) != 0)
//   {
//     index->stats->queries_avg_eff_epsilon =  ((double) index->stats->queries_sum_eff_epsilon)
//                                                / (queries_count - index->stats->eff_epsilon_queries_count)  ;
//     index->stats->queries_sum_squares_eff_epsilon -= (pow(index->stats->queries_sum_eff_epsilon,2)
// 							/(queries_count - index->stats->eff_epsilon_queries_count) );
//     index->stats->queries_sd_eff_epsilon =  sqrt(((double) index->stats->queries_sum_squares_eff_epsilon)
// 						   / (queries_count - index->stats->eff_epsilon_queries_count));
//   }

// }

// void print_queries_stats(isax_index * index)
// {
//   /*
//         printf("---------------------------------------- \n");
//         printf("QUERY SUMMARY STATISTICS \n");
//         printf("TOTAL QUERIES LOADED %u \n", count_queries );
//         printf("TOTAL QUERIES WITH NON-ZERO DISTANCE %u \n", index->stats->total_queries_count );
//         printf("---------------------------------------- \n");
//   */

//         printf("Queries_filter_input_time_secs \t  %f \n", index->stats->queries_filter_input_time/1000000);
//         printf("Queries_filter_output_time_secs \t  %f \n", index->stats->queries_filter_output_time/1000000);
//         printf("Queries_filter_load_node_time_secs \t  %f \n", index->stats->queries_filter_load_node_time/1000000);
//         printf("Queries_filter_cpu_time_secs \t  %f \n", index->stats->queries_filter_cpu_time/1000000);
//         printf("Queries_filter_total_time_secs \t  %f \n", index->stats->queries_filter_total_time/1000000);

//         printf("Queries_filter_seq_input_count \t  %llu  \n", index->stats->queries_filter_seq_input_count);
//         printf("Queries_filter_seq_output_count \t  %llu \n", index->stats->queries_filter_seq_output_count);
//         printf("Queries_filter_rand_input_count \t  %llu  \n", index->stats->queries_filter_rand_input_count);
//         printf("Queries_filter_rand_onput_count \t  %llu \n", index->stats->queries_filter_rand_output_count);

//         printf("Queries_refine_input_time_secs \t  %f \n", index->stats->queries_refine_input_time/1000000);
//         printf("Queries_refine_output_time_secs \t  %f \n", index->stats->queries_refine_output_time/1000000);
//         printf("Queries_refine_load_node_time_secs \t  %f \n", index->stats->queries_refine_load_node_time/1000000);
//         printf("Queries_refine_cpu_time_secs \t  %f \n", index->stats->queries_refine_cpu_time/1000000);
//         printf("Queries_refine_total_time_secs \t  %f \n", index->stats->queries_refine_total_time/1000000);

//         printf("Queries_refine_seq_input_count \t  %llu  \n", index->stats->queries_refine_seq_input_count);
//         printf("Queries_refine_seq_output_count \t  %llu \n", index->stats->queries_refine_seq_output_count);
//         printf("Queries_refine_rand_input_count \t  %llu  \n", index->stats->queries_refine_rand_input_count);
//         printf("Queries_refine_rand_onput_count \t  %llu \n", index->stats->queries_refine_rand_output_count);

//         printf("Queries_total_input_time_secs \t  %f \n", index->stats->queries_total_input_time/1000000);
//         printf("Queries_total_output_time_secs \t  %f \n", index->stats->queries_total_output_time/1000000);
//         printf("Queries_total_Load_node_time_secs \t  %f \n", index->stats->total_load_node_time/1000000);
//         printf("Queries_total_cpu_time_secs \t  %f \n", index->stats->queries_total_cpu_time/1000000);
//         printf("Queries_total_Total_time_secs \t  %f \n", index->stats->queries_total_time/1000000);

//         printf("Queries_total_seq_input_count \t  %llu  \n", index->stats->queries_total_seq_input_count);
//         printf("Queries_total_seq_output_count \t  %llu \n", index->stats->queries_total_seq_output_count);
//         printf("Queries_total_rand_input_count \t  %llu  \n", index->stats->queries_total_rand_input_count);
//         printf("Queries_total_rand_onput_count \t  %llu \n", index->stats->queries_total_rand_output_count);

//         printf("Queries_avg_checked_nodes_count \t  %f \n", index->stats->queries_avg_checked_nodes_count);
//         printf("Queries_avg_checked_ts_count \t  %f \n", index->stats->queries_avg_checked_ts_count);
//         printf("Queries_avg_loaded_nodes_count \t  %f \n", index->stats->queries_avg_loaded_nodes_count);
//         printf("Queries_avg_loaded_ts_count \t  %f \n", index->stats->queries_avg_loaded_ts_count);

// 	printf("Queries_min_tlb \t  %f \n", index->stats->queries_min_tlb);
// 	printf("Queries_max_tlb \t  %f \n", index->stats->queries_max_tlb);
// 	printf("Queries_avg_tlb \t  %f \n", index->stats->queries_avg_tlb);
// 	printf("Queries_sd_tlb \t  %f \n", index->stats->queries_sd_tlb);

// 	printf("Queries_min_pruning_ratio \t  %f \n", index->stats->queries_min_pruning_ratio);
// 	printf("Queries_max_pruning_ratio \t  %f \n", index->stats->queries_max_pruning_ratio);
// 	printf("Queries_avg_pruning_ratio \t  %f \n", index->stats->queries_avg_pruning_ratio);
// 	printf("Queries_sd_pruning_ratio \t  %f \n", index->stats->queries_sd_pruning_ratio);

// 	printf("Queries_min_eff_epsilon \t  %f \n", index->stats->queries_min_eff_epsilon);
// 	printf("Queries_max_eff_epsilon \t  %f \n", index->stats->queries_max_eff_epsilon);
// 	printf("Queries_avg_eff_epsilon \t  %f \n", index->stats->queries_avg_eff_epsilon);
// 	printf("Queries_sd_eff_epsilon \t  %f \n", index->stats->queries_sd_eff_epsilon);

//         printf("Total Combined indexing and querying times \t  %f \n",
// 	       (index->stats->queries_total_time+index->stats->idx_total_time)/1000000);

// }

enum response init_stats(isax_index *index)
{
    index->stats = malloc(sizeof(struct stats_info));
    if (index->stats == NULL)
    {
        fprintf(stderr, "Error in dstree_index.c: Could not allocate memory for stats structure.\n");
        return FAILURE;
    }

    /*INDEX STATISTICS*/
    index->stats->idx_building_input_time = 0;
    index->stats->idx_building_output_time = 0;
    index->stats->idx_building_cpu_time = 0;
    index->stats->idx_building_total_time = 0;

    index->stats->idx_building_seq_input_count = 0;
    index->stats->idx_building_seq_output_count = 0;
    index->stats->idx_building_rand_input_count = 0;
    index->stats->idx_building_rand_output_count = 0;

    index->stats->idx_writing_input_time = 0;
    index->stats->idx_writing_output_time = 0;
    index->stats->idx_writing_cpu_time = 0;
    index->stats->idx_writing_total_time = 0;

    index->stats->idx_writing_seq_input_count = 0;
    index->stats->idx_writing_seq_output_count = 0;
    index->stats->idx_writing_rand_input_count = 0;
    index->stats->idx_writing_rand_output_count = 0;

    index->stats->idx_reading_input_time = 0;
    index->stats->idx_reading_output_time = 0;
    index->stats->idx_reading_cpu_time = 0;
    index->stats->idx_reading_total_time = 0;

    index->stats->idx_reading_seq_input_count = 0;
    index->stats->idx_reading_seq_output_count = 0;
    index->stats->idx_reading_rand_input_count = 0;
    index->stats->idx_reading_rand_output_count = 0;

    index->stats->idx_total_input_time = 0;
    index->stats->idx_total_output_time = 0;
    index->stats->idx_total_cpu_time = 0;
    index->stats->idx_total_time = 0;

    index->stats->idx_total_seq_input_count = 0;
    index->stats->idx_total_seq_output_count = 0;
    index->stats->idx_total_rand_input_count = 0;
    index->stats->idx_total_rand_output_count = 0;

    index->stats->total_nodes_count = 0;
    index->stats->leaf_nodes_count = 0;
    index->stats->empty_leaf_nodes_count = 0;

    index->stats->idx_size_bytes = 0;
    index->stats->idx_size_blocks = 0;

    index->stats->leaves_sizes = NULL;
    index->stats->min_fill_factor = FLT_MAX;
    index->stats->max_fill_factor = 0;
    index->stats->sum_fill_factor = 0;
    index->stats->sum_squares_fill_factor = 0;
    index->stats->avg_fill_factor = 0;
    index->stats->sd_fill_factor = 0;

    index->stats->leaves_heights = NULL;
    index->stats->min_height = FLT_MAX;
    index->stats->max_height = 0;
    index->stats->sum_height = 0;
    index->stats->sum_squares_height = 0;
    index->stats->avg_height = 0;
    index->stats->sd_height = 0;

    index->stats->leaves_counter = 0;

    /*PER QUERY STATISTICS*/
    index->stats->query_filter_input_time = 0;
    index->stats->query_filter_output_time = 0;
    index->stats->query_filter_load_node_time = 0;
    index->stats->query_filter_cpu_time = 0;
    index->stats->query_filter_total_time = 0;

    index->stats->query_filter_seq_input_count = 0;
    index->stats->query_filter_seq_output_count = 0;
    index->stats->query_filter_rand_input_count = 0;
    index->stats->query_filter_rand_output_count = 0;

    index->stats->query_filter_loaded_nodes_count = 0;
    index->stats->query_filter_checked_nodes_count = 0;
    index->stats->query_filter_loaded_ts_count = 0;
    index->stats->query_filter_checked_ts_count = 0;

    index->stats->query_refine_input_time = 0;
    index->stats->query_refine_output_time = 0;
    index->stats->query_refine_load_node_time = 0;
    index->stats->query_refine_cpu_time = 0;
    index->stats->query_refine_total_time = 0;

    index->stats->query_refine_seq_input_count = 0;
    index->stats->query_refine_seq_output_count = 0;
    index->stats->query_refine_rand_input_count = 0;
    index->stats->query_refine_rand_output_count = 0;

    index->stats->query_refine_loaded_nodes_count = 0;
    index->stats->query_refine_checked_nodes_count = 0;
    index->stats->query_refine_loaded_ts_count = 0;
    index->stats->query_refine_checked_ts_count = 0;

    index->stats->query_total_input_time = 0;
    index->stats->query_total_output_time = 0;
    index->stats->query_total_load_node_time = 0;
    index->stats->query_total_cpu_time = 0;
    index->stats->query_total_time = 0;

    index->stats->query_total_seq_input_count = 0;
    index->stats->query_total_seq_output_count = 0;
    index->stats->query_total_rand_input_count = 0;
    index->stats->query_total_rand_output_count = 0;

    index->stats->query_total_loaded_nodes_count = 0;
    index->stats->query_total_checked_nodes_count = 0;
    index->stats->query_total_loaded_ts_count = 0;
    index->stats->query_total_checked_ts_count = 0;
    //    index->stats->loaded_records_count = 0;

    index->stats->query_exact_distance = 0;
    index->stats->query_exact_node_filename = NULL;
    index->stats->query_exact_node_size = 0;
    index->stats->query_exact_node_level = 0;

    index->stats->query_approx_distance = 0;
    index->stats->query_approx_node_filename = NULL;
    index->stats->query_approx_node_size = 0;
    index->stats->query_approx_node_level = 0;

    index->stats->query_lb_distance = 0;

    index->stats->query_tlb = 0;
    index->stats->query_eff_epsilon = 0;
    index->stats->query_pruning_ratio = 0;

    /*SUMMARY STATISTICS FOR ALL QUERIES*/
    index->stats->queries_refine_input_time = 0;
    index->stats->queries_refine_output_time = 0;
    index->stats->queries_refine_load_node_time = 0;
    index->stats->queries_refine_cpu_time = 0;
    index->stats->queries_refine_total_time = 0;

    index->stats->queries_refine_seq_input_count = 0;
    index->stats->queries_refine_seq_output_count = 0;
    index->stats->queries_refine_rand_input_count = 0;
    index->stats->queries_refine_rand_output_count = 0;

    index->stats->queries_filter_input_time = 0;
    index->stats->queries_filter_output_time = 0;
    index->stats->queries_filter_load_node_time = 0;
    index->stats->queries_filter_cpu_time = 0;
    index->stats->queries_filter_total_time = 0;

    index->stats->queries_filter_seq_input_count = 0;
    index->stats->queries_filter_seq_output_count = 0;
    index->stats->queries_filter_rand_input_count = 0;
    index->stats->queries_filter_rand_output_count = 0;

    index->stats->queries_total_input_time = 0;
    index->stats->queries_total_output_time = 0;
    index->stats->queries_total_load_node_time = 0;
    index->stats->queries_total_cpu_time = 0;
    index->stats->queries_total_time = 0;

    index->stats->queries_total_seq_input_count = 0;
    index->stats->queries_total_seq_output_count = 0;
    index->stats->queries_total_rand_input_count = 0;
    index->stats->queries_total_rand_output_count = 0;

    index->stats->queries_min_eff_epsilon = FLT_MAX;
    index->stats->queries_max_eff_epsilon = 0;
    index->stats->queries_sum_eff_epsilon = 0;
    index->stats->queries_sum_squares_eff_epsilon = 0;
    index->stats->queries_avg_eff_epsilon = 0;
    index->stats->queries_sd_eff_epsilon = 0;

    index->stats->queries_min_pruning_ratio = FLT_MAX;
    index->stats->queries_max_pruning_ratio = 0;
    index->stats->queries_sum_pruning_ratio = 0;
    index->stats->queries_sum_squares_pruning_ratio = 0;
    index->stats->queries_avg_pruning_ratio = 0;
    index->stats->queries_sd_pruning_ratio = 0;

    index->stats->queries_min_tlb = FLT_MAX;
    index->stats->queries_max_tlb = 0;
    index->stats->queries_sum_tlb = 0;
    index->stats->queries_sum_squares_tlb = 0;
    index->stats->queries_avg_tlb = 0;
    index->stats->queries_sd_tlb = 0;

    index->stats->tlb_ts_count = 0;
    index->stats->eff_epsilon_queries_count = 0;

    index->stats->queries_avg_checked_nodes_count = 0;
    index->stats->queries_avg_checked_ts_count = 0;
    index->stats->queries_avg_loaded_nodes_count = 0;
    index->stats->queries_avg_loaded_ts_count = 0;

    /*COMBINED STATISTICS FOR INDEXING AND QUERY WORKLOAD*/
    index->stats->total_input_time = 0;
    index->stats->total_output_time = 0;
    index->stats->total_load_node_time = 0;
    index->stats->total_cpu_time = 0;
    index->stats->total_time = 0;
    index->stats->total_time_sanity_check = 0;

    index->stats->total_seq_input_count = 0;
    index->stats->total_seq_output_count = 0;
    index->stats->total_rand_input_count = 0;
    index->stats->total_rand_output_count = 0;

    index->stats->total_parse_time = 0;
    index->stats->total_ts_count = 0;

    return SUCCESS;
}

void get_index_stats(isax_index *index)
{
    index->stats->total_seq_input_count = index->stats->idx_building_seq_input_count + index->stats->idx_writing_seq_input_count + index->stats->idx_reading_seq_input_count;
    //    + index->stats->queries_total_seq_input_count;
    index->stats->total_seq_output_count = index->stats->idx_building_seq_output_count + index->stats->idx_writing_seq_output_count + index->stats->idx_reading_seq_output_count;
    //+ index->stats->queries_total_seq_output_count;
    index->stats->total_rand_input_count = index->stats->idx_building_rand_input_count + index->stats->idx_writing_rand_input_count + index->stats->idx_reading_rand_input_count;
    // + index->stats->queries_total_rand_input_count;
    index->stats->total_rand_output_count = index->stats->idx_building_rand_output_count + index->stats->idx_writing_rand_output_count + index->stats->idx_reading_rand_output_count;
    //+ index->stats->queries_total_rand_output_count;

    index->stats->total_input_time = index->stats->idx_building_input_time + index->stats->idx_writing_input_time + index->stats->idx_reading_input_time;
    //   + index->stats->queries_total_input_time;
    index->stats->total_output_time = index->stats->idx_building_output_time + index->stats->idx_writing_output_time + index->stats->idx_reading_output_time;
    //    + index->stats->queries_total_output_time;
    index->stats->total_cpu_time = index->stats->idx_building_cpu_time + index->stats->idx_writing_cpu_time + index->stats->idx_reading_cpu_time;
    //    + index->stats->queries_total_cpu_time;

    index->stats->total_time = index->stats->total_input_time + index->stats->total_output_time + index->stats->total_cpu_time;

    // index->stats->total_time_sanity_check = total_time;

    // index->stats->load_node_time = load_node_time;
    index->stats->total_parse_time = total_parse_time;

    // index->stats->loaded_nodes_count = loaded_nodes_count;
    index->stats->leaf_nodes_count = leaf_nodes_count;
    index->stats->empty_leaf_nodes_count = empty_leaf_nodes_count;

    // index->stats->checked_nodes_count = checked_nodes_count;
    index->stats->total_nodes_count = total_nodes_count;
    index->stats->total_ts_count = total_ts_count;

    get_index_footprint(index);
}

void get_index_footprint(isax_index *index)
{

    const char *filename = malloc(sizeof(char) * (strlen(index->settings->root_directory) + 15));
    filename = strcpy(filename, index->settings->root_directory);
    filename = strcat(filename, "/index.idx\0");

    struct stat st;
    unsigned int count_leaves;

    if (stat(filename, &st) == 0)
    {
        index->stats->idx_size_bytes = (long long)st.st_size;
        index->stats->idx_size_blocks = (long long)st.st_blksize;
    }

    count_leaves = index->stats->leaf_nodes_count;

    index->stats->avg_fill_factor = ((double)index->stats->sum_fill_factor) / count_leaves;
    index->stats->sum_squares_fill_factor -= (pow(index->stats->sum_fill_factor, 2) / count_leaves);
    index->stats->sd_fill_factor = sqrt(((double)index->stats->sum_squares_fill_factor) / count_leaves);

    index->stats->avg_height = ((double)index->stats->sum_height) / count_leaves;
    index->stats->sum_squares_height -= (pow(index->stats->sum_height, 2) / count_leaves);
    index->stats->sd_height = sqrt(((double)index->stats->sum_squares_height) / count_leaves);

    free(filename);
}

void print_index_stats(isax_index *index, char *dataset)
{
    /*
    printf("------------------------ \n");
    printf("INDEX SUMMARY STATISTICS \n");
    printf("------------------------ \n");
    */
    //  id = -1 for index and id = query_id for queries
    int id = -1;
    printf("Index_building_input_time_secs\t%lf\t%s\t%d\n",
           index->stats->idx_building_input_time / 1000000,
           dataset,
           id);
    printf("Index_building_output_time_secs\t%lf\t%s\t%d\n",
           index->stats->idx_building_output_time / 1000000,
           dataset,
           id);
    printf("Index_building_cpu_time_secs\t%lf\t%s\t%d\n",
           index->stats->idx_building_cpu_time / 1000000,
           dataset,
           id);
    printf("Index_building_total_time_secs\t%lf\t%s\t%d\n",
           index->stats->idx_building_total_time / 1000000,
           dataset,
           id);

    printf("Index_building_seq_input_count\t%llu\t%s\t%d\n",
           index->stats->idx_building_seq_input_count,
           dataset,
           id);

    printf("Index_building_seq_output_count\t%llu\t%s\t%d\n",
           index->stats->idx_building_seq_output_count,
           dataset,
           id);

    printf("Index_building_rand_input_count\t%llu\t%s\t%d\n",
           index->stats->idx_building_rand_input_count,
           dataset,
           id);

    printf("Index_building_rand_output_count\t%llu\t%s\t%d\n",
           index->stats->idx_building_rand_output_count,
           dataset,
           id);

    printf("Index_writing_input_time_secs\t%lf\t%s\t%d\n",
           index->stats->idx_writing_input_time / 1000000,
           dataset,
           id);

    printf("Index_writing_output_time_secs\t%lf\t%s\t%d\n",
           index->stats->idx_writing_output_time / 1000000,
           dataset,
           id);

    printf("Index_writing_cpu_time_secs\t%lf\t%s\t%d\n",
           index->stats->idx_writing_cpu_time / 1000000,
           dataset,
           id);

    printf("Index_writing_total_time_secs\t%lf\t%s\t%d\n",
           index->stats->idx_writing_total_time / 1000000,
           dataset,
           id);

    printf("Index_writing_seq_input_count\t%llu\t%s\t%d\n",
           index->stats->idx_writing_seq_input_count,
           dataset,
           id);

    printf("Index_writing_seq_output_count\t%llu\t%s\t%d\n",
           index->stats->idx_writing_seq_output_count,
           dataset,
           id);

    printf("Index_writing_rand_input_count\t%llu\t%s\t%d\n",
           index->stats->idx_writing_rand_input_count,
           dataset,
           id);

    printf("Index_writing_rand_output_count\t%llu\t%s\t%d\n",
           index->stats->idx_writing_rand_output_count,
           dataset,
           id);

    printf("Index_reading_input_time_secs\t%lf\t%s\t%d\n",
           index->stats->idx_reading_input_time / 1000000,
           dataset,
           id);
    printf("Index_reading_output_time_secs\t%lf\t%s\t%d\n",
           index->stats->idx_reading_output_time / 1000000,
           dataset,
           id);
    printf("Index_reading_cpu_time_secs\t%lf\t%s\t%d\n",
           index->stats->idx_reading_cpu_time / 1000000,
           dataset,
           id);
    printf("Index_reading_total_time_secs\t%lf\t%s\t%d\n",
           index->stats->idx_reading_total_time / 1000000,
           dataset,
           id);

    printf("Index_reading_seq_input_count\t%llu\t%s\t%d\n",
           index->stats->idx_reading_seq_input_count,
           dataset,
           id);

    printf("Index_reading_seq_output_count\t%llu\t%s\t%d\n",
           index->stats->idx_reading_seq_output_count,
           dataset,
           id);

    printf("Index_reading_rand_input_count\t%llu\t%s\t%d\n",
           index->stats->idx_reading_rand_input_count,
           dataset,
           id);

    printf("Index_reading_rand_output_count\t%llu\t%s\t%d\n",
           index->stats->idx_reading_rand_output_count,
           dataset,
           id);

    printf("Index_total_input_time_secs\t%lf\t%s\t%d\n",
           index->stats->total_input_time / 1000000,
           dataset,
           id);
    printf("Index_total_output_time_secs\t%lf\t%s\t%d\n",
           index->stats->total_output_time / 1000000,
           dataset,
           id);
    printf("Index_total_cpu_time_secs\t%lf\t%s\t%d\n",
           index->stats->total_cpu_time / 1000000,
           dataset,
           id);
    printf("Index_total_time_secs\t%lf\t%s\t%d\n",
           index->stats->total_time / 1000000,
           dataset,
           id);

    printf("Index_total_seq_input_count\t%llu\t%s\t%d\n",
           index->stats->total_seq_input_count,
           dataset,
           id);

    printf("Index_total_seq_output_count\t%llu\t%s\t%d\n",
           index->stats->total_seq_output_count,
           dataset,
           id);

    printf("Index_total_rand_input_count\t%llu\t%s\t%d\n",
           index->stats->total_rand_input_count,
           dataset,
           id);

    printf("Index_total_rand_output_count\t%llu\t%s\t%d\n",
           index->stats->total_rand_output_count,
           dataset,
           id);

    printf("Internal_nodes_count\t%lu\t%s\t%d\n",
           (index->stats->total_nodes_count - index->stats->leaf_nodes_count),
           dataset,
           id);

    printf("Leaf_nodes_count\t%lu\t%s\t%d\n",
           index->stats->leaf_nodes_count,
           dataset,
           id);

    printf("Empty_leaf_nodes_count\t%lu\t%s\t%d\n",
           index->stats->empty_leaf_nodes_count,
           dataset,
           id);

    printf("Total_nodes_count\t%lu\t%s\t%d\n",
           index->stats->total_nodes_count,
           dataset,
           id);

    double size_MB = (index->stats->idx_size_bytes) * 1.0 / (1024 * 1024);

    printf("Index_size_MB\t%lf\t%s\t%d\n",
           size_MB,
           dataset,
           id);

    printf("Minimum_fill_factor\t%f\t%s\t%d\n",
           index->stats->min_fill_factor,
           dataset,
           id);

    printf("Maximum_fill_factor\t%f\t%s\t%d\n",
           index->stats->max_fill_factor,
           dataset,
           id);

    printf("Average_fill_factor\t%f\t%s\t%d\n",
           index->stats->avg_fill_factor,
           dataset,
           id);

    printf("SD_fill_factor\t%f\t%s\t%d\n",
           index->stats->sd_fill_factor,
           dataset,
           id);

    printf("Minimum_height\t%u\t%s\t%d\n",
           index->stats->min_height,
           dataset,
           id);

    printf("Maximum_height\t%u\t%s\t%d\n",
           index->stats->max_height,
           dataset,
           id);

    printf("Average_height\t%f\t%s\t%d\n",
           index->stats->avg_height,
           dataset,
           id);

    printf("SD_height\t%f\t%s\t%d\n",
           index->stats->sd_height,
           dataset,
           id);

    printf("Total_ts_count\t%u\t%s\t%d\n",
           index->stats->total_ts_count,
           dataset,
           id);

    for (int i = 0; i < index->stats->leaves_counter; ++i)
    {
        double fill_factor = ((double)index->stats->leaves_sizes[i]) / index->settings->max_leaf_size;
        printf("Leaf_report_node_%d \t Height  %d  \t%s\t%d\n",
               (i + 1),
               index->stats->leaves_heights[i],
               dataset,
               id);
        printf("Leaf_report_node_%d \t Fill_Factor  %f \t%s\t%d\n",
               (i + 1),
               fill_factor,
               dataset,
               id);
    }
}

void print_tlb_stats(isax_index *index, unsigned int query_num, char *queries)
{

    printf("Query_avg_node_tlb\t%lf\t%s\t%u\n",
           total_node_tlb / total_ts_count,
           queries,
           query_num);
    printf("Query_avg_data_tlb\t%lf\t%s\t%u\n",
           total_data_tlb / total_ts_count,
           queries,
           query_num);
    printf("Leaf_nodes_count\t%lu\t%s\t%u\n",
           leaf_nodes_count,
           queries,
           query_num);

    printf("Total_ts_count\t%lu\t%s\t%u\n",
           total_ts_count,
           queries,
           query_num);
}
