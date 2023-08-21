//
// Created by hrxiong on 2/6/22.
//
#ifndef PROJECT_query_US_DTW
#define PROJECT_query_US_DTW

#include <vector>
#include <map>
#include <utility>
#include "utils.h"
#include "dataLoader.h"

// typedef struct EnvInfo {
//     file_position_type startIndex, minl;
//     ts_type PAAmax[SEGMENTS], PAAmin[SEGMENTS];
//     ts_type ex, ex2, meanMin, meanMax, stdMin, stdMax;
// }EnvInfo;

class segmentTree
{

public:
    file_position_type n;
    std::vector<ts_type> mn, mx;
    segmentTree();
    segmentTree(std::vector<ts_type> &arrMin, std::vector<ts_type> &arrMax, file_position_type n);

    inline file_position_type ls(file_position_type x) { return x << 1; }
    inline file_position_type rs(file_position_type x) { return x << 1 | 1; }

    std::pair<ts_type, ts_type> query(file_position_type x, file_position_type y);
    std::pair<ts_type, ts_type> query(file_position_type x, file_position_type y, file_position_type l, file_position_type r, file_position_type p);
};

class query_US_DTW
{
    file_position_type minl, maxl;

    int window_r;

    std::map<std::pair<file_position_type, file_position_type>, std::pair<ts_type *, ts_type *>> cache, cachePAA;
    // cache: [minl, maxl] -> USQl, USQu;
    // cachePAA: [minl, maxl] -> USQPAAl, USQPAAu;

    std::vector<segmentTree *> segTrees, segTreesPAA;

public:
    std::vector<ts_type *> USQs, USQPAAs; // len -> query scaled

    std::vector<ts_type *> USQlbs, USQubs, USQPAAlbs, USQPAAubs; // len -> bounds

    query_US_DTW();

    query_US_DTW(file_position_type minl, file_position_type maxl, int window_r);

    ~query_US_DTW();

    ts_type *uniformScaling(ts_type *data, file_position_type oril, file_position_type tarl);

    ts_type *getPAA(ts_type *query, file_position_type ql);

    void processQuery(ts_type *query, file_position_type ql);

    std::pair<ts_type *, ts_type *> getBounds(file_position_type minl, file_position_type maxl);

    std::pair<ts_type *, ts_type *> getBoundsPAA(file_position_type minl, file_position_type maxl);

    void clear();
};

#endif // PROJECT_query_US_DTW
