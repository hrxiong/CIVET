#include "proxy_function.h"

void compress_write_pointers(FILE *file, int window_size, file_position_type* start_pos, int* min_len, int minl, int W, int H){
    int i, curbyte = 0;
    file_position_type delta;
    unsigned char bmsk=127;
    static unsigned char bytearray[100000];
    
    fwrite(&window_size, sizeof(int), 1, file);
    // first start position
    fwrite(start_pos, sizeof(file_position_type), 1, file);
    // printf("%d ", start_pos[0]);
    for(i = 1; i < window_size; i++){
        delta = (start_pos[i] - start_pos[i-1]) / W;
        // printf("%d ", delta);
        do {
            bytearray[curbyte] = delta & bmsk;
            delta >>= 7;
            if(delta){
                bytearray[curbyte] |= 128;
            }
            curbyte++;
        } while(delta);
        // printf("%d\n", curbyte);
    }
    fwrite(bytearray, sizeof(unsigned char), curbyte, file);

    for(i = 0; i < window_size; i++){
        bytearray[i] = (min_len[i] - minl) / H;
    }
    fwrite(bytearray, sizeof(unsigned char), window_size, file);
}

void read_compressed_pointers(FILE *file, int *window_size, file_position_type* start_pos, int* min_len, int minl, int W, int H){
    int i, curbyte = 0, off;
    file_position_type delta;
    unsigned char bmsk=127, bt;
    static unsigned char bytearray[100000];
    
    fread(window_size, sizeof(int), 1, file);
    // printf("%d\n", *window_size);
    // first start position
    fread(start_pos, sizeof(file_position_type), 1, file);
    // printf("====="); 
    for(i = 1; i < *window_size; i++){
        delta = 0;
        off = 0;
        do {
            fread(&bt, 1, 1, file);
            // printf("%d ", bt);
            delta |= ((int)bt & bmsk) << off;
            off += 7;
        } while(bt&128);
        // printf("===%d\n", delta);
        // printf("\n", bt);
        start_pos[i] = delta * W + start_pos[i-1];
    }

    fread(bytearray, sizeof(unsigned char), *window_size, file);
    for(i = 0; i < *window_size; i++){
        min_len[i] = bytearray[i] * H + minl;
    }
}

void load_env_from_file(FILE *env_file, isax_node_record *record, isax_index *index)
{
    fread(record->sax, sizeof(sax_type), index->settings->paa_segments, env_file);
    fread(record->sax_lower, sizeof(sax_type), index->settings->paa_segments, env_file);
    fread(record->sax_upper, sizeof(sax_type), index->settings->paa_segments, env_file);
    
    // printf("==\n");
    read_compressed_pointers(env_file, &(record->window_size), record->start_pos, record->min_len, index->settings->minl, index->settings->W, index->settings->H);
    // printf("==%d\n", record->window_size);
    // fread(&(record->window_size), sizeof(int), 1, env_file);
    // fread(record->start_pos, sizeof(file_position_type), record->window_size, env_file);
    // fread(record->min_len, sizeof(int), record->window_size, env_file);
}

void write_node_buffer_into_file(FILE *file, isax_node *node, isax_index_settings *settings)
{
    int i;
    for (i = 0; i < node->env_buffer_size; i++)
    {
        fwrite(node->sax_lower_buffer[i], sizeof(sax_type), settings->paa_segments, file);
        fwrite(node->sax_upper_buffer[i], sizeof(sax_type), settings->paa_segments, file);
        // printf("test");
        // printf("%d ")
        compress_write_pointers(file, node->window_size_buffer[i], node->start_pos_buffer[i], node->min_len_buffer[i], settings->minl, settings->W, settings->H);
        // fwrite(&node->window_size_buffer[i], settings->window_size_byte_size, 1, file);
        // fwrite(node->start_pos_buffer[i], sizeof(file_position_type), settings->window_size, file);
        // fwrite(node->min_len_buffer[i], sizeof(int), settings->window_size, file);
    }
}

int read_node_files(FILE *sax_file, FILE *ts_file, isax_node_record *record, isax_index *index)
{
    if (!fread(record->sax, sizeof(sax_type), index->settings->paa_segments, sax_file) ||
        !fread(record->sax_lower, sizeof(sax_type), index->settings->paa_segments, ts_file) ||
        !fread(record->sax_upper, sizeof(sax_type), index->settings->paa_segments, ts_file))
    {
        return 0;
    }
    // printf("%d\n", record->sax_lower[0]);
    // ||
    // !fread(&record->window_size, index->settings->window_size_byte_size, 1, ts_file) ||
    // !fread(record->start_pos, sizeof(file_position_type), index->settings->window_size, ts_file) ||
    // !fread(record->min_len, sizeof(int), index->settings->window_size, ts_file)
    read_compressed_pointers(ts_file, &(record->window_size), record->start_pos, record->min_len, index->settings->minl, index->settings->W, index->settings->H);
    // printf("%d\n", record->window_size);
    // printf("wtfwtf\n");

    // fread(record->sax, sizeof(sax_type), index->settings->paa_segments, sax_file);
    // // printf("test2\n");
    // // printf("%d\n", record->sax[0]);
    // fread(record->sax_upper, sizeof(sax_type), index->settings->paa_segments, ts_file);
    // // printf("%d\n", record->sax_upper[0]);
    // // printf("test3\n");
    // fread(&record->window_size, index->settings->window_size_byte_size, 1, ts_file);
    // // printf("%d\n", record->window_size);
    // // printf("test4\n");
    // fread(record->start_pos, sizeof(file_position_type), index->settings->window_size_byte_size, ts_file);
    // // printf("%d\n", record->start_pos[0]);
    // // printf("test5\n");
    // fread(record->min_len, sizeof(int), index->settings->window_size_byte_size, ts_file);
    // // printf("%d\n", record->min_len[0]);
    // // printf("test6\n");

    return 1;
}

void update_node_sax_lower_upper(isax_node *node, sax_type *sax_lower, sax_type *sax_upper, int sax_segments)
{
    int i;
    if (node->sax_upper_max_values == NULL)
    {
        node->sax_upper_max_values = malloc(sizeof(sax_type *) * sax_segments);
        node->sax_lower_max_values = malloc(sizeof(sax_type *) * sax_segments);
        for (i = 0; i < sax_segments; i++)
        {
            node->sax_upper_max_values[i] = sax_upper[i];
            node->sax_lower_max_values[i] = sax_lower[i];
        }
    }
    else
    {
        for (i = 0; i < sax_segments; i++)
        {
            if (sax_upper[i] > node->sax_upper_max_values[i])
            {
                node->sax_upper_max_values[i] = sax_upper[i];
            }
            if (sax_lower[i] < node->sax_lower_max_values[i])
            {
                node->sax_lower_max_values[i] = sax_lower[i];
            }
        }
    }
}

void set_isax_lower_upper(isax_node *node, isax_index *index)
{
    while (node != NULL)
    {
        /// Change the isax values of Upper bound

        if (node->parent)
        {
            if (node->sax_upper_max_values != NULL)
            {
                int i;
                for (i = 0; i < index->settings->paa_segments; i++)
                {
                    root_mask_type mask_upper = 0x00;
                    root_mask_type mask_lower = 0x00;

                    int k;
                    for (k = 0; k <= node->parent->split_data->split_mask[i]; k++)
                    {
                        mask_upper |= (index->settings->bit_masks[index->settings->sax_bit_cardinality - 1 - k] &
                                       node->sax_upper_max_values[i]);
                        mask_lower |= (index->settings->bit_masks[index->settings->sax_bit_cardinality - 1 - k] &
                                       node->sax_lower_max_values[i]);
                    }
                    mask_upper = mask_upper >> (index->settings->sax_bit_cardinality - node->parent->split_data->split_mask[i] - 1);
                    mask_lower = mask_lower >> (index->settings->sax_bit_cardinality - node->parent->split_data->split_mask[i] - 1);
                    node->isax_upper_values[i] = (int)mask_upper;
                    node->isax_lower_values[i] = (int)mask_lower;
                }
                free(node->sax_upper_max_values);
                free(node->sax_lower_max_values);
                node->sax_upper_max_values = NULL;
                node->sax_lower_max_values = NULL;
            }
        }
        // If it has no parent it is root node and as such it's cardinality is 1.
        else
        {
            root_mask_type mask_upper = 0x00;
            root_mask_type mask_lower = 0x00;
            int i;
            for (i = 0; i < index->settings->paa_segments; i++)
            {
                mask_upper = (index->settings->bit_masks[index->settings->sax_bit_cardinality - 1] & node->sax_upper_max_values[i]);
                mask_upper = mask_upper >> (index->settings->sax_bit_cardinality - 1);
                node->isax_upper_values[i] = (int)mask_upper;

                mask_lower = (index->settings->bit_masks[index->settings->sax_bit_cardinality - 1] & node->sax_lower_max_values[i]);
                mask_lower = mask_upper >> (index->settings->sax_bit_cardinality - 1);
                node->isax_lower_values[i] = (int)mask_lower;
            }

            free(node->sax_upper_max_values);
            free(node->sax_lower_max_values);
            node->sax_upper_max_values = NULL;
            node->sax_lower_max_values = NULL;
        }

        // delete all pre file 
        if (node->is_leaf && node->filename)
        {
            char *fname = malloc(sizeof(char) * (strlen(node->filename) + 5));
            strcpy(fname, node->filename);
            strcat(fname, ".pre");
            FILE *tmp_file;
            if (tmp_file = fopen(fname, "r"))
            {
                fclose(tmp_file);
                remove(fname);
            }
            free(fname);
        }

        isax_node *r_child = node->right_child;
        isax_node *l_child = node->left_child;

        if (r_child != NULL)
        {
            set_isax_lower_upper(r_child, index);
        }

        if (l_child != NULL)
        {
            set_isax_lower_upper(l_child, index);
        }
        node = node->next;
    }
}

isax_index *build_isax_index(const char *root_directory, int timeseries_size,
                             int paa_segments, int sax_bit_cardinality,
                             int max_leaf_size, int initial_leaf_buffer_size,
                             unsigned long long max_total_buffer_size, int initial_fbl_buffer_size,
                             int new_index, int window_size, int minl, int maxl, int W, int H)
{
    isax_index_settings *index_settings = isax_index_settings_init(root_directory,           // INDEX DIRECTORY
                                                                   timeseries_size,          // TIME SERIES SIZE
                                                                   paa_segments,             // PAA SEGMENTS
                                                                   sax_bit_cardinality,      // SAX CARDINALITY IN BITS
                                                                   max_leaf_size,            // LEAF SIZE
                                                                   initial_leaf_buffer_size, // INITIAL LEAF BUFFER SIZE
                                                                   max_total_buffer_size,    // FLUSH LIMIT
                                                                   initial_fbl_buffer_size,  // INITIAL FBL BUFFER SIZE
                                                                   new_index,                // USE INDEX
                                                                   window_size,             // WINDOW SIZE
                                                                   minl, maxl, W, H);             

    isax_index *index = isax_index_init(index_settings);



    isax_index_binary_file("dataset", index);

    int already_finalized = 0;
    isax_index_finalize(index, NULL, &already_finalized);


    return index;
}

void delete_isax_index(isax_index *index, char use_index)
{

    isax_index_destroy(index, use_index, NULL);
    if (index->settings->bit_masks != NULL)
    {
        free(index->settings->bit_masks);
    }
    if (use_index)
    {
        free(index->stats->leaves_heights);
        free(index->stats->leaves_sizes);
    }
    if (index->stats != NULL)
    {
        free(index->stats);
    }

    if (index->settings->max_sax_cardinalities != NULL)
    {
        free(index->settings->max_sax_cardinalities);
    }

    if (index->settings != NULL)
    {
        free(index->settings);
    }
    free(index);
}