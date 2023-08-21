#include "globals.h"

double tS;
double tE;

struct timeval total_time_start;
struct timeval parse_time_start;
struct timeval input_time_start;
struct timeval output_time_start;
struct timeval load_node_start;
struct timeval current_time;
struct timeval fetch_start;
struct timeval fetch_check_start;
double total_input_time;
double load_node_time;
double total_output_time;
double total_parse_time;
double total_time;


/*
int total_tree_nodes;
int loaded_nodes;
int checked_nodes;
file_position_type loaded_records;
*/


struct timeval partial_time_start;
struct timeval partial_input_time_start;
struct timeval partial_output_time_start;
struct timeval partial_load_node_time_start;         

double partial_time;
double partial_input_time;
double partial_output_time;
double partial_load_node_time;

unsigned long long partial_seq_input_count;
unsigned long long partial_seq_output_count;
unsigned long long partial_rand_input_count;
unsigned long long partial_rand_output_count;

unsigned long total_nodes_count;
unsigned long leaf_nodes_count;
unsigned long empty_leaf_nodes_count;
unsigned long loaded_nodes_count;
unsigned long checked_nodes_count;
unsigned long loaded_ts_count;
unsigned long checked_ts_count;
unsigned long total_ts_count;
unsigned long total_queries_count;
ts_type total_node_tlb;
ts_type total_data_tlb;


int FLUSHES;