//
// Created by hrxiong on 2/6/22.
//
#ifndef PROJECT_fastEnv_US_DTW_H
#define PROJECT_fastEnv_US_DTW_H

#include <vector>
#include <queue>
#include <map>
#include "utils.h"
#include "dataLoader.h"
#include "query_US_DTW.h"

extern "C"
{
#include "isax_node_record.h"
#include "isax_node.h"
#include "proxy_function.h"
#include "isax_index.h"
}

// typedef struct EnvInfo {
//     file_position_type startIndex, minl;
//     ts_type PAAmax[SEGMENTS], PAAmin[SEGMENTS];
//     ts_type ex, ex2, meanMin, meanMax, stdMin, stdMax;
// }EnvInfo;

typedef struct NodeDist
{
    ts_type distance;
    ts_type distance2;
    isax_node *node;
} NodeDist;

class fastEnv_US_DTW
{
public:
    ts_type *Q; // query array
    ts_type *D; // array of TS data

    EnvelopeLoader *envLoader;
    query_US_DTW *queryHelper;

    isax_index *index;
    std::string indexFile;

    long long allEnvCnt, calEnvCnt;
    long long allSCnt, calSCnt;

    std::priority_queue<ts_type> rq;
    int knn_k;
    bool rq_full;

    int window_r;



    // std::vector<ts_type *> USQs;
    // std::vector<file_position_type> USQsLength;

    // std::map<file_position_type, std::vector<ts_type> > USQl, USQu; // mapping minl -> l, u
    // std::map<file_position_type, std::vector<ts_type> > USQPAAl, USQPAAu; // mapping minl -> PAAmin, PAAmax

    // std::vector<EnvInfo> envs;

    //    std::vector<std::map<file_position_type, std::vector<ts_type> > > dataInfo;// i-th starting position, mapping l -> ex, ex2(for l - 1), u min, u max, sigma min, sigma max

    //    ts_type scalingFactor;  // factor of scaling
    ts_type bsf;       // best-so-far
    ts_type envFactor; // correcting the LBenv caused by different env lengths. minEnvLen / (minEnvLen + 1)
    int m;             // length of query
    int beta, gamma;
    int maxl, minl;
    file_position_type dl;
    file_position_type loc = 0; // answer: location of the best-so-far match
    int len;

    double pruningPower1, pruningPower2;

    double approximateTime;

    double error_ratio;

    fastEnv_US_DTW();

    fastEnv_US_DTW(int beta, int gamma, int minl, int maxl, int k, int window_r, isax_index *index, std::string indexFile);

    // fastEnv_US_DTW(ts_type *data, file_position_type dLength, ts_type *query, file_position_type ql, file_position_type meta, file_position_type gamma, file_position_type minl, file_position_type maxl);

    ~fastEnv_US_DTW();

    void initialQuery(ts_type *query, int ql);

    void initialData(ts_type *data, file_position_type dLength);

    void isax_to_sax(sax_type *isax_lower, sax_type *isax_upper, sax_type *sax_lower, sax_type *sax_upper, sax_type *sax_cardinalities);

    // void preprocessQuery();

    // void loadEnvs();

    inline ts_type getBsf() { return bsf; }
    inline double getPP1() { return pruningPower1; }
    inline double getPP2() { return pruningPower2; }

    inline file_position_type getLoc() { return loc; }

    // ts_type *uniformScaling(ts_type *data, file_position_type oril, file_position_type tarl);

    void calculate_distance_for_env_with_pos_minl(file_position_type env_start_pos, int env_minl, bool is_exact);

    //    ts_type lowerBound();
    ts_type lowerBound(int length, ts_type *ql, ts_type *qu, file_position_type startPos, ts_type &umin, ts_type &umax, ts_type &smin, ts_type &smax);

    ts_type lowerBoundEnv(int envMinl, ts_type *ql, ts_type *qu, sax_type *l, sax_type *u);

    ts_type lb_keogh_cumulative_norm(ts_type *t, ts_type *uo, ts_type *lo, ts_type *cb, int len, ts_type mean, ts_type std);

    // ts_type distance(const int length, const ts_type *q, const file_position_type &j, const ts_type &mean, const ts_type &std);

    ts_type distance(ts_type* A, ts_type* B, ts_type *cb, int lengthQuery, int window_r, ts_type mean, ts_type stdDev);


    bool approximate_search();

    void run();
};

#endif // PROJECT_fastEnv_US_DTW_H
