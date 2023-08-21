#ifndef proxy_function_h
#define proxy_function_h

#include "config.h"
#include "globals.h"
#include <stdio.h>
#include <stdlib.h>
#include "isax_node_record.h"
#include "isax_index.h"
#include "isax_node.h"
#include "isax_file_loaders.h"

void load_env_from_file(FILE *env_file, isax_node_record *record, isax_index *index);
void write_node_buffer_into_file(FILE *file, isax_node *node, isax_index_settings *settings);
int read_node_files(FILE *sax_file, FILE *ts_file, isax_node_record *record, isax_index *index);
void update_node_sax_lower_upper(isax_node *node, sax_type *sax_lower, sax_type *sax_upper, int sax_segments);
void set_isax_lower_upper(isax_node *node, isax_index *index);

isax_index *build_isax_index(const char *root_directory, int timeseries_size,
                             int paa_segments, int sax_bit_cardinality,
                             int max_leaf_size, int initial_leaf_buffer_size,
                             unsigned long long max_total_buffer_size, int initial_fbl_buffer_size,
                             int new_index, int window_size,
                             int minl, int maxl, int W, int H);

void delete_isax_index(isax_index *index, char use_index);

void compress_write_pointers(FILE *file, int window_size, file_position_type* start_pos, int* min_len, int minl, int W, int H);
void read_compressed_pointers(FILE *file, int *window_size, file_position_type* start_pos, int* min_len, int minl, int W, int H);



#endif