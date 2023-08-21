#include "query_US_DTW.h"

#include <cmath>
#include <functional>
#include <iostream>

using namespace std;

query_US_DTW::query_US_DTW() = default;

query_US_DTW::query_US_DTW(file_position_type minl, file_position_type maxl, int window_r) : 
    minl(minl), maxl(maxl), USQs(maxl + 1, NULL), USQPAAs(maxl + 1, NULL), window_r(window_r),
    USQlbs(maxl + 1, NULL), USQubs(maxl + 1, NULL), USQPAAlbs(maxl + 1, NULL), USQPAAubs(maxl + 1, NULL)
{
}

query_US_DTW::~query_US_DTW()
{
    clear();
}

ts_type *query_US_DTW::uniformScaling(ts_type *data, file_position_type oril, file_position_type tarl)
{
    ts_type *ret = (ts_type *)malloc(sizeof(ts_type) * tarl);
    file_position_type idx;
    for (file_position_type i = 1; i <= tarl; i++)
    {
        idx = static_cast<file_position_type>(ceil(ts_type(oril) * i / tarl) - 1);
        ret[i - 1] = data[idx];
    }
    return ret;
}

ts_type *query_US_DTW::getPAA(ts_type *query, file_position_type ql)
{
    ts_type *paa = (ts_type *)malloc(sizeof(ts_type) * SEGMENTS);
    ts_type tmp;

    for (int k = 0; k < SEGMENTS; k++)
    {
        file_position_type left, right;
        left = (k * ql + SEGMENTS - 1) / SEGMENTS;
        right = (k * ql + ql + SEGMENTS - 1) / SEGMENTS;

        tmp = 0;
        for (file_position_type x = left; x < right; x++)
        {
            tmp += query[x];
        }
        tmp /= right - left;

        paa[k] = tmp;
    }

    return paa;
}

void query_US_DTW::processQuery(ts_type *query, file_position_type ql)
{
    // cout << "Begin processing query" << endl;
    for (file_position_type tarl = minl; tarl <= maxl; tarl += 1)
    {
        USQs[tarl] = uniformScaling(query, ql, tarl);
        norm(USQs[tarl], tarl);

        USQlbs[tarl] = (ts_type *)malloc(sizeof(ts_type) * maxl);
        USQubs[tarl] = (ts_type *)malloc(sizeof(ts_type) * maxl);
        for(int tmpi = 0; tmpi < maxl; tmpi++){
            USQlbs[tarl][tmpi] = INF;
            USQubs[tarl][tmpi] = -INF;
        }

        lower_upper_lemire(USQs[tarl], tarl, window_r, USQlbs[tarl], USQubs[tarl]);
        // for(int tmpi = 0; tmpi < tarl; tmpi++){
        //     cout << USQlbs[tarl][tmpi] << " " << USQubs[tarl][tmpi] << endl;
        // }
        // exit(1);
        USQPAAs[tarl] = getPAA(USQs[tarl], tarl);
        USQPAAlbs[tarl] = getPAA(USQlbs[tarl], tarl);
        USQPAAubs[tarl] = getPAA(USQubs[tarl], tarl);
        // for(int tmpi = 0; tmpi < SEGMENTS; tmpi++){
        //     cout << USQPAAlbs[tarl][tmpi] << " " << USQPAAubs[tarl][tmpi] << endl;
        // }
        // exit(1);
    }

    segTrees.resize(maxl);
    for (file_position_type i = 0; i < maxl; i++)
    {
        vector<ts_type> arrMin(maxl + 1, INF), arrMax(maxl + 1, -INF);
        // for(auto p: USQs){
        for (int tmpidx = minl; tmpidx <= maxl; tmpidx += 1)
        {
            if (i >= tmpidx)
                continue;
            arrMin[tmpidx] = USQlbs[tmpidx][i];
            arrMax[tmpidx] = USQubs[tmpidx][i];
        }
        segTrees[i] = new segmentTree(arrMin, arrMax, maxl);
    }

    segTreesPAA.resize(SEGMENTS);
    for (file_position_type i = 0; i < SEGMENTS; i++)
    {
        vector<ts_type> arrMin(maxl + 1, INF), arrMax(maxl + 1, -INF);
        // for(auto p: USQPAAs){
        for (int tmpidx = minl; tmpidx <= maxl; tmpidx += 1)
        {
            arrMin[tmpidx] = USQPAAlbs[tmpidx][i];
            arrMax[tmpidx] = USQPAAubs[tmpidx][i];
        }
        segTreesPAA[i] = new segmentTree(arrMin, arrMax, maxl);
    }

    // cout << "Done" << endl;
}

pair<ts_type *, ts_type *> query_US_DTW::getBounds(file_position_type minl, file_position_type maxl)
{
    if (cache.count({minl, maxl}))
        return cache[{minl, maxl}];

    ts_type *lb, *ub;

    lb = (ts_type *)malloc(sizeof(ts_type) * minl);
    ub = (ts_type *)malloc(sizeof(ts_type) * minl);
    for (int i = 0; i < minl; i++)
    {
        auto [mn, mx] = segTrees[i]->query(minl, maxl);
        lb[i] = mn;
        ub[i] = mx;
    }

    return cache[{minl, maxl}] = pair<ts_type *, ts_type *>({lb, ub});
}

pair<ts_type *, ts_type *> query_US_DTW::getBoundsPAA(file_position_type minl, file_position_type maxl)
{
    if (cachePAA.count({minl, maxl}))
        return cachePAA[{minl, maxl}];

    ts_type *paalb, *paaub;

    paalb = (ts_type *)malloc(sizeof(ts_type) * SEGMENTS);
    paaub = (ts_type *)malloc(sizeof(ts_type) * SEGMENTS);
    for (int i = 0; i < SEGMENTS; i++)
    {
        auto [mn, mx] = segTreesPAA[i]->query(minl, maxl);
        paalb[i] = mn;
        paaub[i] = mx;
    }

    return cachePAA[{minl, maxl}] = pair<ts_type *, ts_type *>({paalb, paaub});
}

void query_US_DTW::clear()
{
    // cout << cachePAA.size() << endl;
    for (auto &p : USQs)
    {
        if (p)
            free(p);
    }

    for (auto &p : USQlbs)
    {
        if (p)
            free(p);
    }

    for (auto &p : USQubs)
    {
        if (p)
            free(p);
    }

    for (auto &p : USQPAAs)
    {
        if (p)
            free(p);
    }

    for (auto &p : USQPAAlbs)
    {
        if (p)
            free(p);
    }

    for (auto &p : USQPAAubs)
    {
        if (p)
            free(p);
    }

    for (auto &t : segTrees)
        delete t;
    for (auto &t : segTreesPAA)
        delete t;

    // // delete[] segTrees; ?

    for (auto &p : cache)
    {
        auto [lb, ub] = p.second;
        free(lb);
        free(ub);
    }
    cache.clear();
    for (auto &p : cachePAA)
    {
        auto [lbPAA, ubPAA] = p.second;
        // cout << lbPAA[0] << " " << ubPAA[0] << endl;
        free(lbPAA);
        free(ubPAA);
    }
    cachePAA.clear();
}

segmentTree::segmentTree(vector<ts_type> &arrMin, vector<ts_type> &arrMax, file_position_type n) : n(n), mn(n << 2, INF), mx(n << 2, -INF)
{
    function<void(file_position_type, file_position_type, file_position_type)> build = [&](file_position_type p, file_position_type l, file_position_type r)
    {
        if (l == r)
        {
            mn[p] = arrMin[l];
            mx[p] = arrMax[l];
            return;
        }
        file_position_type mid = (l + r) >> 1;
        build(ls(p), l, mid);
        build(rs(p), mid + 1, r);
        mn[p] = min(mn[ls(p)], mn[rs(p)]);
        mx[p] = max(mx[ls(p)], mx[rs(p)]);
    };
    build(1, 1, n);
}

pair<ts_type, ts_type> segmentTree::query(file_position_type x, file_position_type y)
{
    return query(x, y, 1, n, 1);
}

pair<ts_type, ts_type> segmentTree::query(file_position_type x, file_position_type y, file_position_type l, file_position_type r, file_position_type p)
{
    ts_type retmn = INF, retmx = -INF;
    if (x <= l && r <= y)
        return {mn[p], mx[p]};
    file_position_type mid = (l + r) >> 1;
    if (x <= mid)
    {
        auto tmpp = query(x, y, l, mid, ls(p));
        retmn = min(retmn, tmpp.first);
        retmx = max(retmx, tmpp.second);
    }
    if (y > mid)
    {
        auto tmpp = query(x, y, mid + 1, r, rs(p));
        retmn = min(retmn, tmpp.first);
        retmx = max(retmx, tmpp.second);
    }
    return {retmn, retmx};
}
