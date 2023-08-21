//
// Created by hrxiong on 2/6/22.
//

#include "fastEnv_US_DTW.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <queue>

#include <sys/time.h>

#include <ctime>

#define dist(x,y) ((x-y)*(x-y))

using namespace std;

long long record_cnt_approximate, record_cnt_exact;

fastEnv_US_DTW::fastEnv_US_DTW(int beta, int gamma, int minl, int maxl, int k, int window_r, isax_index *index, string indexFile) : 
    beta(beta), gamma(gamma), minl(minl), maxl(maxl), knn_k(k), index(index), indexFile(indexFile), window_r(window_r)
{
    envFactor = minl / gamma;
    envFactor = envFactor / (envFactor + 1);

    // envLoader = new EnvelopeLoader(indexFile);
    queryHelper = new query_US_DTW(minl, maxl, window_r);


    // cout << envLoader->envNum << endl;
}

// fastEnv_US_DTW::fastEnv_US_DTW(ts_type *data, file_position_type dLength, ts_type *query, file_position_type ql, file_position_type beta, file_position_type gamma, file_position_type minl, file_position_type maxl)
//         :
//         D(data), dl(dLength), Q(query), m(ql), beta(beta), gamma(gamma), minl(minl), maxl(maxl) {

//     envFactor = minl / gamma;
//     envFactor = envFactor / (envFactor + 1);

//     // preprocessQuery();
//     // loadEnvs();
//     queryHelper = new query_US_DTW(minl, maxl);
//     envLoader = new EnvelopeLoader("testfile");

//     queryHelper->processQuery(query, ql);
// }

fastEnv_US_DTW::~fastEnv_US_DTW() {}

void fastEnv_US_DTW::initialData(ts_type *data, file_position_type dLength)
{
    D = data;
    dl = dLength;
    // loadEnvs();
}

void fastEnv_US_DTW::initialQuery(ts_type *query, int ql)
{
    queryHelper->clear();

    Q = query;
    m = ql;

    queryHelper->processQuery(query, ql);

}

// ts_type
// fastEnv_US_DTW::distance(const int length, const ts_type *q, const file_position_type &startPos, const ts_type &mean,
//                         const ts_type &std)
// {
//     int i;
//     ts_type sum = 0, x;
//     for (i = 0; i < length && sum < bsf * length; i++)
//     {
//         x = (D[i + startPos] - mean) / std;
//         sum += (x - q[i]) * (x - q[i]);
//     }
//     return sum / length;
// }

ts_type
fastEnv_US_DTW::distance(ts_type* A, ts_type* B, ts_type *cb, int lengthQuery, int window_r, ts_type mean, ts_type stdDev)
{

    ts_type *cost;
    ts_type *cost_prev;
    ts_type *cost_tmp;
    int i,j,k;
    ts_type x,y,z,min_cost;

    /// Instead of using matrix of size O(m^2) or O(mr), we will reuse two array of size O(r).
    cost = (ts_type*)malloc(sizeof(ts_type)*(2*window_r+1));
    for(k=0; k<2*window_r+1; k++)    cost[k]=MAXFLOAT;

    cost_prev = (ts_type*)malloc(sizeof(ts_type)*(2*window_r+1));
    for(k=0; k<2*window_r+1; k++)    cost_prev[k]=MAXFLOAT;

    for (i=0; i<lengthQuery; i++)
    {
        k = max(0,window_r-i);
        min_cost = MAXFLOAT;

        for(j=max(0,i-window_r); j<=min(lengthQuery-1,i+window_r); j++, k++)
        {
            /// Initialize all row and column
            if ((i==0)&&(j==0))
            {
                ts_type normA= (A[0]-mean)/stdDev;
                cost[k]=dist(normA,B[0]);
                min_cost = cost[k];
                continue;
            }

            if ((j-1<0)||(k-1<0))     y = MAXFLOAT;
            else                      y = cost[k-1];
            if ((i-1<0)||(k+1>2*window_r))   x = MAXFLOAT;
            else                      x = cost_prev[k+1];
            if ((i-1<0)||(j-1<0))     z = MAXFLOAT;
            else                      z = cost_prev[k];

            /// Classic DTW calculation
            ts_type normA_i= (A[i]-mean)/stdDev;
            cost[k] = min( min( x, y) , z) + dist(normA_i,B[j]);

            /// Find minimum cost in row for early abandoning (possibly to use column instead of row).
            if (cost[k] < min_cost)
            {   min_cost = cost[k];
            }
        }

        /// We can abandon early if the current cummulative distace with lower bound together are larger than bsf
        if (i+window_r < lengthQuery-1 && min_cost + cb[i+window_r+1] >= bsf * lengthQuery)
        {   free(cost);
            free(cost_prev);
            //return min_cost + cb[i+window_r+1];
            return -1;
        }

        /// Move current array to previous array.
        cost_tmp = cost;
        cost = cost_prev;
        cost_prev = cost_tmp;
    }
    k--;

    /// the DTW distance is in the last cell in the matrix of size O(m^2) or at the middle of our array.
    ts_type final_dtw = cost_prev[k];
    free(cost);
    free(cost_prev);
    return final_dtw / lengthQuery;
}

ts_type fastEnv_US_DTW::lowerBound(int length, ts_type *ql, ts_type *qu, file_position_type startPos, ts_type &umin, ts_type &umax, ts_type &smin, ts_type &smax)
{
    //    cout << umin << " " << umax << " " << smin << " " << smax << endl;
    ts_type ret = 0;
    ts_type xmin = INF, xmax = -INF, d;
    for (int i = 0; i < length; i++)
    {
        d = D[startPos + i];
        xmin = d > umax ? (d - umax) / smax : (d - umax) / smin;
        xmax = d > umin ? (d - umin) / smin : (d - umin) / smax;

        if (qu[i] < xmin)
        {
            d = qu[i] - xmin;
            ret += d * d;
        }
        else if (ql[i] > xmax)
        {
            d = ql[i] - xmax;
            ret += d * d;
        }
        if (ret > bsf * (length + gamma))
            break;
    }
    return ret / (length + gamma);
}

ts_type fastEnv_US_DTW::lowerBoundEnv(int envMinl, ts_type *ql, ts_type *qu, sax_type *l, sax_type *u)
{
    //    cout << umin << " " << umax << " " << smin << " " << smax << endl;
    ts_type ret = 0, d;
    double envFactor = envMinl / gamma;
    envFactor /= envFactor + 1;
    // cout << "test1" << endl;
    ts_type li, ui;
    for (int k = 0; k < SEGMENTS; k++)
    {
        // cout << l[k] << endl;
        li = getSAXLowerBP(l[k]);
        ui = getSAXUpperBP(u[k]);
        if (qu[k] < li)
        {
            d = qu[k] - li;
            ret += d * d;
        }
        else if (ql[k] > ui)
        {
            d = ql[k] - ui;
            ret += d * d;
        }
        if (ret * envFactor > bsf * SEGMENTS)
            break;
    }
    // cout << "test2" << endl;
    return ret * envFactor / SEGMENTS;
}

ts_type fastEnv_US_DTW::lb_keogh_cumulative_norm(ts_type *t, ts_type *uo, ts_type *lo, ts_type *cb, int len, ts_type mean, ts_type std)
{
    ts_type lb = 0;
    ts_type x, d;
    int i;
    for (i = 0; i < len && lb < bsf * len; i++)
    {
        x = (t[i] - mean) / std;
        d = 0;
        if (x > uo[i])
            d = dist(x,uo[i]);
        else if(x < lo[i])
            d = dist(x,lo[i]);
        lb += d;
        cb[i] = d;
    }
    return lb / len;
}

void fastEnv_US_DTW::calculate_distance_for_env_with_pos_minl(file_position_type env_start_pos, int env_minl, bool is_exact)
{

    file_position_type i = 0, j;

    ts_type ex, ex2, curex, curex2;
    file_position_type curEnd, curLen;
    ts_type mean, std, lb, dist;

    ts_type * cbFirst = (ts_type *) malloc(maxl*sizeof(ts_type));
    ts_type * cb = (ts_type *) malloc(maxl*sizeof(ts_type));

    ex = 0;
    ex2 = 0;
    for (i = env_start_pos; i < env_start_pos + env_minl - 1; i++)
    {
        ex += D[i];
        ex2 += D[i] * D[i];
    }
    auto [USQl, USQu] = queryHelper->getBounds(env_minl, env_minl + gamma - 1);
    for (i = env_start_pos; i < env_start_pos + beta && i <= dl - minl; i++)
    {
        if (is_exact)
        {
            allSCnt++;
        }
        j = i + env_minl - 1;

        ts_type meanMin = INF, meanMax = -INF, stdMin = INF, stdMax = -INF;

        curex = ex;
        curex2 = ex2;
        curEnd = j;
        while (curEnd - j < gamma && curEnd < dl && curEnd < i + maxl)
        {
            curex += D[curEnd];
            curex2 += D[curEnd] * D[curEnd];
            curEnd++;
            curLen = (curEnd - i);

            mean = curex / curLen;
            std = curex2 / curLen;
            std = sqrt(std - mean * mean);

            if (mean > meanMax)
                meanMax = mean;
            // else if(mean < meanMin) meanMin = mean;
            if (mean < meanMin)
                meanMin = mean;
            if (std > stdMax)
                stdMax = std;
            // else if(std < stdMin) stdMin = std;
            if (std < stdMin)
                stdMin = std;
        }

        // lb for ith
        // lb = lowerBound(env.minl, USQl, USQu, i, env.meanMin, env.meanMax, env.stdMin, env.stdMax);
        lb = lowerBound(env_minl, USQl, USQu, i, meanMin, meanMax, stdMin, stdMax);

        // lb = -INF;
        // cout << env.minl << " " << lb << " " << bsf << endl;
        if (lb > bsf)
        {
            ex += D[j] - D[i];
            ex2 += D[j] * D[j] - D[i] * D[i];
            continue;
        }

        if (is_exact)
        {
            calSCnt++;
        }

        // cal distance for ith

        curex = ex;
        curex2 = ex2;
        curEnd = j;
        //            cout << curex << " " << curex2 << endl;
        // while(curEnd - j < gamma && curEnd < dl && curEnd < i + maxl){

        while (curEnd - j < gamma && curEnd < dl && curEnd < i + maxl)
        {

            curex += D[curEnd];
            curex2 += D[curEnd] * D[curEnd];
            curEnd++;
            //
            curLen = (curEnd - i);

            mean = curex / curLen;
            std = curex2 / curLen;
            std = sqrt(std - mean * mean);


            ts_type lbKeogh = lb_keogh_cumulative_norm(&D[i], queryHelper->USQubs[curLen], queryHelper->USQlbs[curLen], cbFirst, curLen, mean, std);
            if(lbKeogh > bsf)
                continue;
            
            int w;
            // compute the correct 
            cb[curLen-1]= cbFirst[curLen-1];
            for(w=curLen-2; w>=0; w--)
            {
                cb[w] = cb[w+1]+cbFirst[w];
            }
            
            dist = distance(&D[i], queryHelper->USQs[curLen], cb, curLen, window_r, mean, std);
            // cout << dist << endl; 

            // dist = distance(curLen, queryHelper->USQs[curLen], i, mean, std);

            if (dist >= 0 && dist < bsf)
            {
                rq.push(dist);
                rq.pop();
                bsf = rq.top();
                loc = i;
                len = curLen;
            }
        }

        ex += D[j] - D[i];
        ex2 += D[j] * D[j] - D[i] * D[i];
    }
    free(cbFirst);
    free(cb);
}

void fastEnv_US_DTW::isax_to_sax(sax_type *isax_lower, sax_type *isax_upper, sax_type *sax_lower, sax_type *sax_upper, sax_type *sax_cardinalities)
{
    int c_c, c_m = index->settings->sax_bit_cardinality;
    // sax_type region_lower = v << (c_m - c_c);
    // sax_type region_upper = (~((int)FLT_MAX << (c_m - c_c)) | region_lower);
    for (int k = 0; k < index->settings->paa_segments; k++)
    {
        c_c = sax_cardinalities[k];
        sax_lower[k] = isax_lower[k] << (c_m - c_c);
        sax_upper[k] = (isax_upper[k] << (c_m - c_c)) |
                       (~((unsigned long long)-1 << (c_m - c_c)));
    }
}

bool fastEnv_US_DTW::approximate_search()
{
    bool isExact = false;
    auto comp = [](const NodeDist *a, const NodeDist *b) -> bool
    {
        if (a->distance == b->distance)
        {
            return a->distance2 > b->distance2;
        }
        return a->distance > b->distance;
    };

    ts_type lb;
    sax_type *sax_lower, *sax_upper;
    priority_queue<NodeDist *, vector<NodeDist *>, decltype(comp)> pq(comp);
    auto [USQPAAl, USQPAAu] = queryHelper->getBoundsPAA(minl, maxl - 1);
    isax_node_record *record;
    // Access raw data with random I/O?

    bsf = FLT_MAX;

    record = isax_node_record_init(index->settings->paa_segments,
                                   index->settings->window_size);

    sax_lower = (sax_type *)malloc(sizeof(sax_type) * index->settings->paa_segments);
    sax_upper = (sax_type *)malloc(sizeof(sax_type) * index->settings->paa_segments);

    isax_node *current_root_node = index->first_node;
    while (current_root_node != NULL)
    {
        NodeDist *mindist_result = (NodeDist *)malloc(sizeof(NodeDist));
        isax_to_sax(current_root_node->isax_lower_values, current_root_node->isax_upper_values,
                    sax_lower, sax_upper, current_root_node->isax_cardinalities);
        mindist_result->distance = lowerBoundEnv(minl, USQPAAl, USQPAAu, sax_lower, sax_upper);
        isax_to_sax(current_root_node->isax_values, current_root_node->isax_values,
                    sax_lower, sax_upper, current_root_node->isax_cardinalities);
        mindist_result->distance2 = lowerBoundEnv(minl, USQPAAl, USQPAAu, sax_lower, sax_upper);
        mindist_result->node = current_root_node;
        pq.push(mindist_result);
        current_root_node = current_root_node->next;
    }

    // cout << "Begin main loop in approximate search" << endl;

    NodeDist *n;
    int vis_leaf_cnt = 0;

    while (!pq.empty())
    {
        n = pq.top();
        pq.pop();
        // cout << "Pop out node from pq" << endl;
        if (vis_leaf_cnt > MAX_VISIT_LEAF_SIZE)
        {
            break;
        }
        if (n->distance > bsf)
        {
            isExact = true;
            break;
        }
        if (n->node->is_leaf)
        {
            vis_leaf_cnt++;
            // cout << "Test before leaf node calculation" << endl;
            if (n->node->has_full_data_file)
            {
                string filename(n->node->filename);
                string sax_filename, ts_filename;
                sax_filename = filename + ".sax";
                ts_filename = filename + ".ts";

                FILE *sax_file = fopen(sax_filename.c_str(), "rb");
                FILE *ts_file = fopen(ts_filename.c_str(), "rb");

                // cout << ts_filename << endl;
                if (sax_file == NULL)
                {
                    cout << "Can not open file " << sax_filename << endl;
                    exit(1);
                }
                if (ts_file == NULL)
                {
                    cout << "Can not open file " << ts_filename << endl;
                    exit(1);
                }
                // cout << "fk" << endl;
                // cout << "test1 in approximate search" << endl;
                while (read_node_files(sax_file, ts_file, record, index))
                {
                    // cout << 
                    // cout << "test2 in approximate search" << endl;

                    // cout << "test0" << endl;
                    // cout << record->sax[0] << " " << record->sax_upper[0] << " " << record->window_size << " ";
                    // cout << record->start_pos[0] << " " << record->min_len[0] << endl;
                    int record_min_len, record_max_len;
                    record_min_len = *min_element(record->min_len, record->min_len + record->window_size);
                    record_max_len = *max_element(record->min_len, record->min_len + record->window_size);
                    // cout << "test1" << endl;
                    auto [USQPAAl, USQPAAu] = queryHelper->getBoundsPAA(record_min_len, min(record_max_len + gamma, maxl) - 1);
                    // cout << "test2" << endl;

                    lb = lowerBoundEnv(record_min_len, USQPAAl, USQPAAu, record->sax_lower, record->sax_upper);

                    if (lb > bsf)
                        continue;
                    // cout << "test3" << endl;

                    for (int wi = 0; wi < record->window_size; wi++)
                    {
                        record_cnt_approximate++;
                        // cout << record->start_pos[wi] << " " << record->min_len[wi] << endl;

                        // gettimeofday(&time_start1, NULL);
                        // cout << record->start_pos[wi] << " " << record->min_len[wi] << endl;
                        // cout << record->start_pos[wi] << ' ' << record->min_len[wi] << endl;
                        calculate_distance_for_env_with_pos_minl(record->start_pos[wi], record->min_len[wi], false);
                        // gettimeofday(&time_end1, NULL);

                        // tss = time_start1.tv_sec;
                        // tes = time_end1.tv_sec;
                        // ts = (time_start1.tv_usec);
                        // te = (time_end1.tv_usec);
                        // diff = (tes - tss) + ((te - ts) / 1000000.0);
                        // distanceTime += diff;
                    }
                    // cout << "test4" << endl;
                }

                fclose(sax_file);
                fclose(ts_file);
            }
        }
        else
        {
            ts_type child_distance;
            ts_type child_distance2;
            // cout << "Test before non-leaf node calculation" << endl;

            if (n->node->left_child->isax_cardinalities != NULL)
            {
                isax_to_sax(n->node->left_child->isax_lower_values, n->node->left_child->isax_upper_values,
                            sax_lower, sax_upper, n->node->left_child->isax_cardinalities);
                child_distance = lowerBoundEnv(minl, USQPAAl, USQPAAu, sax_lower, sax_upper);
                isax_to_sax(n->node->left_child->isax_values, n->node->left_child->isax_values,
                            sax_lower, sax_upper, n->node->left_child->isax_cardinalities);
                child_distance2 = lowerBoundEnv(minl, USQPAAl, USQPAAu, sax_lower, sax_upper);

                if (child_distance <= bsf)
                {
                    NodeDist *mindist_result = (NodeDist *)malloc(sizeof(NodeDist));
                    mindist_result->distance = child_distance;
                    mindist_result->distance2 = child_distance2;
                    mindist_result->node = n->node->left_child;
                    pq.push(mindist_result);
                }
            }
            if (n->node->right_child->isax_cardinalities != NULL)
            {
                isax_to_sax(n->node->right_child->isax_lower_values, n->node->right_child->isax_upper_values,
                            sax_lower, sax_upper, n->node->right_child->isax_cardinalities);
                child_distance = lowerBoundEnv(minl, USQPAAl, USQPAAu, sax_lower, sax_upper);
                isax_to_sax(n->node->right_child->isax_values, n->node->right_child->isax_values,
                            sax_lower, sax_upper, n->node->right_child->isax_cardinalities);
                child_distance2 = lowerBoundEnv(minl, USQPAAl, USQPAAu, sax_lower, sax_upper);

                if (child_distance <= bsf)
                {
                    NodeDist *mindist_result = (NodeDist *)malloc(sizeof(NodeDist));
                    mindist_result->distance = child_distance;
                    mindist_result->distance2 = child_distance2;
                    mindist_result->node = n->node->right_child;
                    pq.push(mindist_result);
                }
            }
        }
        free(n);
    }

    // Free the nodes that where not popped.
    while (!pq.empty())
    {
        n = pq.top();
        pq.pop();
        free(n);
    }
    // Free the priority queue.

    isax_node_record_destroy(record);

    free(sax_lower);
    free(sax_upper);

    // gettimeofday(&time_end, NULL);

    // tss = time_start.tv_sec;
    // tes = time_end.tv_sec;
    // ts = (time_start.tv_usec);
    // te = (time_end.tv_usec);
    // diff = (tes - tss) + ((te - ts) / 1000000.0);
    // totalTime = diff;

    // cout << (double)distanceTime / totalTime << " ";

    return isExact;
}

void fastEnv_US_DTW::run()
{
    struct timeval time_start;
    struct timeval time_start1;
    struct timeval time_end;
    struct timeval time_end1;

    long double tss, tes, ts, te, diff;
    long double totalTime, LB2Time = 0, distanceTime = 0;
    approximateTime = 0;

    record_cnt_approximate = 0, record_cnt_exact = 0;

    ts_type lb;

    allEnvCnt = 0, calEnvCnt = 0;
    allSCnt = 0, calSCnt = 0;

    while(rq.size()) rq.pop();
    for(int tmpi = 0; tmpi < knn_k; tmpi++){
        rq.push(INF);
    }
    bsf = INF;

    // approximate search here approximate_search();
    // gettimeofday(&time_start1, NULL);
    // cout << "fkfkfk" << endl;

    double approximate_result;

    approximate_search();
    approximate_result = bsf;

    EnvInfoSAX *ep;
    
    FILE *env_file = fopen(indexFile.c_str(), "rb");
    isax_node_record *record = isax_node_record_init(index->settings->paa_segments, index->settings->window_size);
    // while (ep = envLoader->getNext())
    
    while (!feof(env_file))
    {
        load_env_from_file(env_file, record, index);
        allEnvCnt++;
        // auto &env = *ep;
        int record_min_len, record_max_len;
        record_min_len = *min_element(record->min_len, record->min_len + record->window_size);
        record_max_len = *max_element(record->min_len, record->min_len + record->window_size);

        // auto [USQPAAl, USQPAAu] = queryHelper->getBoundsPAA(env.minl, env.minl + gamma - 1);
        auto [USQPAAl, USQPAAu] = queryHelper->getBoundsPAA(record_min_len, min(record_max_len + gamma, maxl) - 1);

        // gettimeofday(&time_start1, NULL);
        // lb = lowerBoundEnv(env.minl, USQPAAl, USQPAAu, env.SAXmin, env.SAXmax);
        lb = lowerBoundEnv(record_min_len, USQPAAl, USQPAAu, record->sax_lower, record->sax_upper);

        // lb = -INF;
        // cout << lb << " " << bsf << endl;
        if (lb > bsf)
            continue;
        calEnvCnt++;

        // ex = env.ex;
        // ex2 = env.ex2;
        for (int wi = 0; wi < record->window_size; wi++)
        {
            // cout << record->start_pos[wi] << " " << record->min_len[wi] << endl;
            record_cnt_exact++;
            calculate_distance_for_env_with_pos_minl(record->start_pos[wi], record->min_len[wi], true);
        }
        // calculate_distance_for_env_with_pos_minl(env.startIndex, env.minl);
    }

    // gettimeofday(&time_end, NULL);

    // tss = time_start.tv_sec;
    // tes = time_end.tv_sec;
    // ts = (time_start.tv_usec);
    // te = (time_end.tv_usec);
    // diff = (tes - tss) + ((te - ts) / 1000000.0);
    // totalTime = diff;
    // cout << approximateTime / totalTime << " ";
    // cout << record_cnt_approximate << " " << record_cnt_exact << " ";
    // cout << loc << " " << len << endl;
    // cout << endl;
    // while(!rq.empty()){
        // cout << rq.top() << ", ";
        // rq.pop();
    // }
    // cout << " | " << loc << " | " << len << endl;
    // cout << endl;
    // cout << bsf << endl;
    fclose(env_file);
    isax_node_record_destroy(record);

    error_ratio = approximate_result / bsf;

    pruningPower1 = 1 - (double)calEnvCnt / allEnvCnt;
    pruningPower2 = 1 - (double)calSCnt / allSCnt;
}
