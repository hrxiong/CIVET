#include <iostream>

using namespace std;

extern "C"
{
#include "config.h"
#include "globals.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <getopt.h>
#include <time.h>
#include "sax/sax.h"
#include "sax/ts.h"
#include "isax_visualize_index.h"
#include "isax_file_loaders.h"
}

#include "dataLoader.h"
#include "fastEnvelope.h"
#include "fastEnv_US_DTW.h"

int main(int argc, char **argv)
{
    struct timeval time_start;
    struct timeval time_start1;
    struct timeval time_end;
    struct timeval time_end1;

    long double tss, tes, ts, te, diff;
    long double totalTime, approximateTime = 0, LB2Time = 0, distanceTime = 0;

    // std::cout << "test" << std::endl;

    INIT_STATS()
    COUNT_TOTAL_TIME_START

// #if VERBOSE_LEVEL == 0
//     // printf("Executing in silent mode. Please wait.\n");
// #endif

    static char *dataset = "/Users/kostas/Desktop/Repositories/kstzmptn/OnTheFlyIsax/Code/datasets/1416/dataset_big_1416.dat";
    static char *queries = "/Users/kostas/Desktop/Repositories/kstzmptn/OnTheFlyIsax/Code/datasets/1416/dataset_big_1416.dat";
    static char *dataset_hists = "/home/karima/myDisk/data/Cgenerator/data_current_hists.txt";

    static char *index_path = "myexperiment/";
    static int dataset_size = 2000;
    static int queries_size = 2000;
    static int time_series_size = 256;
    static int paa_segments = 8;
    static int sax_cardinality = 8;
    static int leaf_size = 1000;
    static int initial_lbl_size = 10000;
    static unsigned long long flush_limit = 100000;
    static int initial_fbl_size = 10000;
    static char use_index = 0;
    static char calc_tlb = 0;
    static char use_ascii_input = 0;
    static float minimum_distance = FLT_MAX;
    static float epsilon = 0; // by default perform exact search
    static float delta = 1;   // by default perform exact search
    static unsigned int k = 1;
    static char incremental = 0;
    static unsigned int nprobes = 0;
    static int window_size = 10;
    static int env_flush_limit = 100000;
    static int W = 20;
    static int H = 20;
    static int minl = 128;
    static int maxl = 256;
    static int convert_data = 0;
    static int dtw_window_r = 5;
    // static int knn_k = 1;

    while (1)
    {
        static struct option long_options[] = {
            {"ascii-input", no_argument, 0, 'a'},
            {"initial-lbl-size", required_argument, 0, 'b'},
            {"epsilon", required_argument, 0, 'c'},
            {"dataset", required_argument, 0, 'd'},
            {"use-index", no_argument, 0, 'e'},
            {"flush-limit", required_argument, 0, 'f'},
            {"calc-tlb", no_argument, 0, 'g'},
            {"delta", required_argument, 0, 'h'},
            {"initial-fbl-size", required_argument, 0, 'i'},
            {"queries", required_argument, 0, 'j'},
            {"k", required_argument, 0, 'k'},
            {"leaf-size", required_argument, 0, 'l'},
            {"queries-size", required_argument, 0, 'm'},
            {"dataset-hists", required_argument, 0, 'n'},
            {"nprobes", required_argument, 0, 'o'},
            {"index-path", required_argument, 0, 'p'},
            {"incremental", no_argument, 0, 'q'},
            {"env-flush-limit", required_argument, 0, 'r'},
            {"minimum-distance", required_argument, 0, 's'},
            {"timeseries-size", required_argument, 0, 't'},
            {"convert-data", no_argument, 0, 'u'},
            {"window-size", required_argument, 0, 'w'},
            {"sax-cardinality", required_argument, 0, 'x'},
            {"dataset-size", required_argument, 0, 'z'},
            {"help", no_argument, 0, '?'},
            {"W", required_argument, 0, '1'},
            {"H", required_argument, 0, '2'},
            {"minl", required_argument, 0, '3'},
            {"maxl", required_argument, 0, '4'},
            {"dtw-window-size", required_argument, 0, '5'}
        };

        /* getopt_long stores the option index here. */
        int option_index = 0;

        int c = getopt_long(argc, argv, "",
                            long_options, &option_index);
        if (c == -1)
            break;
        switch (c)
        {
        case 'w':
            window_size = atoi(optarg);
            break;
        case 'j':
            queries = optarg;
            break;

        case 's':
            minimum_distance = atof(optarg);
            break;

        case 'm':
            queries_size = atoi(optarg);
            break;

        case 'n':
            dataset_hists = optarg;
            break;

        case '1':
            W = atoi(optarg);
            break;
        case '2':
            H = atoi(optarg);
            break;
        case '3':
            minl = atoi(optarg);
            break;
        case '4':
            maxl = atoi(optarg);
            break;
        case '5':
            dtw_window_r = atoi(optarg);
            break;
        case 'r':
            env_flush_limit = atoi(optarg);
            break;

        case 'k':
            k = atoi(optarg);
            if (k < 1)
            {
                fprintf(stderr, "Please change the k to be greater than 0.\n");
                exit(-1);
            }
            break;

        case 'd':
            dataset = optarg;
            break;

        case 'p':
            index_path = optarg;
            break;

        case 'z':
            dataset_size = atoi(optarg);
            break;

        case 't':
            time_series_size = atoi(optarg);
            break;

        case 'x':
            sax_cardinality = atoi(optarg);
            break;

        case 'l':
            leaf_size = atoi(optarg);
            break;

        case 'b':
            initial_lbl_size = atoi(optarg);
            break;

        case 'f':
            flush_limit = atoi(optarg);
            break;

        case 'g':
            calc_tlb = 1;
            break;

        case 'u':
            convert_data = 1;
            break;

        case 'i':
            initial_fbl_size = atoi(optarg);
            break;
        case '?':
            printf("Usage:\n\
                       \t--dataset XX \t\t\tThe path to the dataset file\n\
                       \t--queries XX \t\t\tThe path to the queries file\n\
                       \t--dataset-size XX \t\tThe number of time series to load\n\
                       \t--queries-size XX \t\tThe number of queries to do\n\
                       \t--minimum-distance XX\t\tThe minimum distance we search (MAX if not set)\n\
                       \t--ascii-input  \t\t\tSpecifies that ascii input files will be used\n\
                       \t--index-path XX \t\tThe path of the output folder\n\
                       \t--timeseries-size XX\t\tThe size of each time series\n\
                       \t--sax-cardinality XX\t\tThe maximum sax cardinality in number of bits (power of two).\n\
                       \t--leaf-size XX\t\t\tThe maximum size of each leaf\n\
                       \t--initial-lbl-size XX\t\tThe initial lbl buffer size for each buffer.\n\
                       \t--flush-limit XX\t\tThe limit of time series in memory at the same time\n\
                       \t--initial-fbl-size XX\t\tThe initial fbl buffer size for each buffer.\n\
                       \t--use-index  \t\t\tSpecifies that an input index will be used\n\
                       \t--help\n\n");
            return 0;
            break;
        case 'a':
            use_ascii_input = 1;
            break;
        case 'e':
            use_index = 1;
            break;
        case 'q':
            incremental = 1;
            break;
        case 'o':
            nprobes = atoi(optarg);
            break;
        case 'c':
            epsilon = atof(optarg);
            break;
        case 'h':
            delta = atof(optarg);
            break;
        default:
            exit(-1);
            break;
        }
    }

    if (convert_data)
    {
        // cout << "begin converting" << endl;
        convert2Bin(dataset);
        convert2Bin(queries);
    }
    else if (use_index == 0)
    {
        gettimeofday(&time_start, NULL);
        BinDataLoader *dataLoader;
        dataLoader = new BinDataLoader(dataset);
        dataLoader->getNext();

        cout << "minl " << minl << ", maxl " << maxl << ", W " << W;
        cout << ", H " << H << ", window size " << window_size << endl;
        string envFilename = string(index_path) + "env.bin";
        envBuilder *myEnvBuilder = new envBuilder(envFilename, minl, maxl, W, H);
        myEnvBuilder->getBulkEnvelopeImproved(dataLoader->timeSeries, dataLoader->dl, window_size, env_flush_limit);
        isax_index *index = build_isax_index(index_path,       // INDEX DIRECTORY
                                             time_series_size, // TIME SERIES SIZE
                                             paa_segments,     // PAA SEGMENTS
                                             sax_cardinality,  // SAX CARDINALITY IN BITS
                                             leaf_size,        // LEAF SIZE
                                             initial_lbl_size, // INITIAL LEAF BUFFER SIZE
                                             flush_limit,      // FLUSH LIMIT
                                             initial_fbl_size, // INITIAL FBL BUFFER SIZE
                                             1,                // new INDEX
                                             window_size,       // WINDOW SIZE
                                             minl, maxl, W, H);     
        index_write(index);

        gettimeofday(&time_end, NULL);
        tss = time_start.tv_sec;
        tes = time_end.tv_sec;
        ts = (time_start.tv_usec);
        te = (time_end.tv_usec);
        diff = (tes - tss) + ((te - ts) / 1000000.0);
        cout << "Index Building Time: " << diff << "s." << endl;
        delete_isax_index(index, use_index);
    } 
    else
    {
        double pp1 = 0, pp2 = 0, er = 0, aptime = 0;

        gettimeofday(&time_start, NULL);
        // isax_index *index;
        isax_index *index = index_read(index_path);
        if (index == NULL)
        {
            fprintf(stderr, "Error main.c:  Could not read the index from disk.\n");
            return -1;
        }

        BinDataLoader *dataLoader = new BinDataLoader(dataset);
        BinDataLoader *queryLoader = new BinDataLoader(queries);

        string envFilename = string(index_path) + "env.bin";
        fastEnv_US_DTW *usedHelper = new fastEnv_US_DTW(W, H, minl, maxl, k, dtw_window_r, index, envFilename);

        dataLoader->getNext();
        usedHelper->initialData(dataLoader->timeSeries, dataLoader->dl);

        int query_cnt = 0;
        

        while (queryLoader->getNext())
        {
            // gettimeofday(&time_start1, NULL);
            query_cnt++;
            // cout << tmp_cnt << endl;
            // if(tmp_cnt <= 26) continue;
            usedHelper->initialQuery(queryLoader->timeSeries, queryLoader->dl);

            usedHelper->run();
            cout << "." ;
            cout.flush();

            pp1 += usedHelper->pruningPower1;
            pp2 += usedHelper->pruningPower2;
            er += usedHelper->error_ratio;
            aptime += usedHelper->approximateTime;
        }
        cout << endl;
        

        gettimeofday(&time_end, NULL);
        tss = time_start.tv_sec;
        tes = time_end.tv_sec;
        ts = (time_start.tv_usec);
        te = (time_end.tv_usec);
        totalTime = (tes - tss) + ((te - ts) / 1000000.0);
        cout << "Average query time: " << totalTime / query_cnt << "s." << endl;
        cout << "Average approximate time: " << aptime / query_cnt << "s." << endl;
        cout << "Average pruning power stage one: " << pp1 / query_cnt << endl;
        cout << "Average pruning power stage two: " << pp2 / query_cnt << endl;
        cout << "Average error ratio: " << er / query_cnt << endl;
        cout << "===========" << endl;
        cout << endl;
    }

    // exit(1);

    // delete_isax_index(index, use_index);
    return 0;
}
