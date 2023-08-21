//
// Created by hrxiong on 30/5/22.
//

#include "fastEnvelope.h"

extern "C"
{
#include "proxy_function.h"
}

#include <iostream>
#include <cstdio>
#include <vector>
#include <cmath>
#include <ctime>
#include <string>
#include <algorithm>
#include <cstdlib>

#include <sys/time.h>

// TODO: new space with malloc
int idxOut[1000][8], idxIn[1000][8];

using namespace std;

struct timeval time_start;
struct timeval time_end;

struct timeval time_start1;
struct timeval time_end1;

long double tss, tes, ts, te, diff;
long double totalTime, testTime;

envBuilder::envBuilder() = default;

envBuilder::envBuilder(string filename, int minl, int maxl, int beta, int gamma) : 
minl(minl), maxl(maxl), beta(beta), gamma(gamma)
{
    outfp = fopen(filename.c_str(), "wb");
    if (outfp == NULL)
    {
        cout << "can not open the file " << filename << endl;
    }
}

void invSax(sax_type *s, sax_type *sax1, sax_type *sax2, int segments, int cardinality)
{
    int j, i, k, segi, invj;
    unsigned long long n;

    for (j = 0; j < segments; j++)
        s[j] = 0;

    segi = 0;
    invj = cardinality - 1;

    for (i = cardinality - 1; i >= 0; i--)
    {
        for (j = 0; j < segments; j++)
        {
            n = sax1[j];
            n = n >> i;

            s[segi] |= (n % 2) << invj;

            invj--;
            if (invj == -1)
            {
                segi++;
                invj = cardinality - 1;
            }

            n = sax2[j];
            n = n >> i;

            s[segi] |= (n % 2) << invj;

            invj--;
            if (invj == -1)
            {
                segi++;
                invj = cardinality - 1;
            }
        }
    }
}

bool comp(EnvInfoSAX &env1, EnvInfoSAX &env2)
{
    if (env1.minl != env2.minl)
        return env1.minl < env2.minl;
    for (int i = 0; i < 2 * SEGMENTS; i++)
    {
        if (env1.invSAX[i] != env2.invSAX[i])
            return env1.invSAX[i] < env2.invSAX[i];
    }
    return env1.startIndex < env2.startIndex;
}

bool comp_idx(EnvInfoSAX &env1, EnvInfoSAX &env2)
{
    return env1.startIndex < env2.startIndex;
}

void envBuilder::flushAllEnv()
{
    int tmpu, tmpl;
    
    for (int i = 0; i < curEnv; i++)
    {
        invSax(saxEnvs[i].invSAX, saxEnvs[i].SAXmax, saxEnvs[i].SAXmin, SEGMENTS, 8);
    }
    sort(saxEnvs, saxEnvs + curEnv, comp);
    int i = 0, numToFlush = 0, j, k;
    while (i < curEnv)
    {
        for (k = 0; k < SEGMENTS; k++)
        {
            // saxMean_flush[k] = 0;
            saxMin_flush[k] = 255;
            saxMax_flush[k] = 0;
        }
        numToFlush = window_size < curEnv - i ? window_size : curEnv - i;

        sort(saxEnvs + i, saxEnvs + i + numToFlush, comp_idx);

        for (j = 0; j < numToFlush; j++)
        {
            for (k = 0; k < SEGMENTS; k++)
            {
                // saxMean_flush[k] += saxEnvs[i + j].SAXkey[k];
                saxMin_flush[k] = min(saxMin_flush[k], saxEnvs[i + j].SAXmin[k]);
                saxMax_flush[k] = max(saxMax_flush[k], saxEnvs[i + j].SAXmax[k]);
            }
            startPos_flush[j] = saxEnvs[i + j].startIndex;
            minLen_flush[j] = saxEnvs[i + j].minl;
        }

        for (k = 0; k < SEGMENTS; k++)
        {
            tmpl = saxMin_flush[k];
            tmpu = saxMax_flush[k];
            if(tmpl==tmpu)
                saxMean_flush[k] = tmpu;
            else
                saxMean_flush[k] = rand() % (tmpu-tmpl) + tmpl;
            // cout << saxMean_flush[k] << " ";
        }
        // cout << endl;

        // cout << numToFlush << endl;

        fwrite(saxMean_flush, sizeof(sax_type), SEGMENTS, outfp);
        fwrite(saxMin_flush, sizeof(sax_type), SEGMENTS, outfp);
        fwrite(saxMax_flush, sizeof(sax_type), SEGMENTS, outfp);

        compress_write_pointers(outfp, numToFlush, startPos_flush, minLen_flush, minl, beta, gamma);

        // fwrite(&numToFlush, sizeof(int), 1, outfp);
        // fwrite(startPos_flush, sizeof(file_position_type), numToFlush, outfp);
        // fwrite(minLen_flush, sizeof(int), numToFlush, outfp);
        envTotalCnt++;

        i += numToFlush;
    }
}

void envBuilder::writeEnvs(int envCnt)
{
    flush_cnt++;
    for (int i = 0; i < envCnt; i++)
    {
        // cout << curEnv << endl;
        saxEnvs[curEnv].setByPAAEnv(envs[i]);
        curEnv++;
    }
    if (flush_cnt % flush_limit == 0)
    {
        flushAllEnv();
        // cout << curEnv << endl;
        flush_cnt = 0;
        curEnv = 0;
    }
    // fwrite(saxEnvs, sizeof(EnvInfoSAX), envCnt, outfp);
}

void envBuilder::prepare(ts_type *data, int minl, int maxl)
{
    int l, r, len;
    memset(idxOut, -1, sizeof(idxOut));
    memset(idxIn, -1, sizeof(idxIn));

    // idxOut.resize(maxl - minl + 1);
    // idxIn.resize(maxl - minl + 1);

    len = minl - 1;

    int curL[SEGMENTS], curR[SEGMENTS];

    for (int i = 0; i < SEGMENTS; i++)
    {
        l = (i * len + SEGMENTS - 1) / SEGMENTS;
        r = (i * len + len + SEGMENTS - 1) / SEGMENTS - 1;
        segLenBase[i] = r - l + 1;

        segLeftIndexBase[i] = l;
        segRightIndexBase[i] = r + 1;

        curL[i] = l;
        curR[i] = r;
        acc[i] = 0;
        for (int idx = l; idx <= r; idx++)
        {
            acc[i] += data[idx];
        }
    }


    for (len = minl; len <= maxl; len++)
    {
        for (int i = 0; i < SEGMENTS; i++)
        {
            l = (i * len + SEGMENTS - 1) / SEGMENTS;
            r = (i * len + len + SEGMENTS - 1) / SEGMENTS - 1;

            for (int tmpidx = curL[i]; tmpidx < l; tmpidx++)
            {
                idxOut[len - minl][i] = tmpidx;
            }
            for (int tmpidx = curR[i]; tmpidx < r; tmpidx++)
            {
                idxIn[len - minl][i] = tmpidx + 1;
            }
            curL[i] = l;
            curR[i] = r;
        }
    }
}

void envBuilder::getBulkEnvelopeImproved(ts_type *data, file_position_type dl, int ws, int fl)
{
    // gettimeofday(&time_start, NULL);
    srand(0);

    window_size = ws;
    flush_limit = fl / beta + 1;
    flush_cnt = 0;
    curEnv = 0;
    envTotalCnt = 0;

    prepare(data, minl, maxl);

    // ofstream idxfs("idx_" + to_string(beta) + "_" + to_string(gamma));
    // ofstream fs("envelope_" + to_string(beta) + "_" + to_string(gamma));

    file_position_type i = 0, j = 0, curEnd = 0, l;

    ts_type ex = 0, ex2 = 0, mean, std, curex, curex2;
    ts_type d;
    bool cut_flag;

    while (j < minl - 1 && j < dl)
    {
        d = data[j++];
        ex += d;
        ex2 += d * d;
    }

    // startIndex minL length PAAmax PAAmin PAAkey ex(for ml-1) ex2(for ml-1) meanMin meanMax stdMin stdMax

    // nums of env between minl and maxl partitioned by gamma
    // minl 40, maxl 47, gamma 3 -> 40 41 42 | 43 44 45 | 46 47

    int maxEnvNum = (maxl - minl) / gamma + 1, ei, envCnt;

    envs = (EnvInfoPAA *)malloc(sizeof(EnvInfoPAA) * maxEnvNum);
    saxEnvs = (EnvInfoSAX *)malloc(sizeof(EnvInfoSAX) * maxEnvNum * (flush_limit + 1));

    startPos_flush = (file_position_type *)malloc(sizeof(file_position_type) * window_size);
    minLen_flush = (int *)malloc(sizeof(int) * window_size);

    // envs.resize(maxEnvNum);
    // saxEnvs.resize(maxEnvNum);
    // vector<file_position_type> keyLengths(maxEnvNum);

    // vector<ts_type> prePAAs(SEGMENTS), PAAs(SEGMENTS);
    vector<ts_type> PAAs(SEGMENTS);
    // ts_type threshold = theta * SEGMENTS, shifts;
    // ts_type threshold2 = theta2 * SEGMENTS;
    unsigned long long totalCnt = 0;
    int k;

    // for (ei = 0; ei < maxEnvNum; ei++)
    // envs[0].reset(0); envCnt = 0;
    envCnt = 0;
    for (i = 0; i <= dl - minl; i++, j++)
    {
        // cout << "i" << endl;
        // if (i % beta == 0) {
        //     for (ei = 0; ei < maxEnvNum; ei++) envs[ei].reset(i);
        //     envCnt = 0;
        // }
        if (i && i % REFLUSHNUM == 0)
        {
            ex = 0;
            ex2 = 0;
            int tmplen = minl - 1;
            int lidx, ridx;
            for (int tmpk = 0; tmpk < SEGMENTS; tmpk++)
            {
                lidx = (tmpk * tmplen + SEGMENTS - 1) / SEGMENTS;
                ridx = (tmpk * tmplen + tmplen + SEGMENTS - 1) / SEGMENTS - 1;
                acc[tmpk] = 0;
                for (file_position_type idx = lidx; idx <= ridx; idx++)
                {
                    acc[tmpk] += data[idx + i];
                    ex += data[idx + i];
                    ex2 += data[idx + i] * data[idx + i];
                }
            }
        }

        // initialize info per step
        for (k = 0; k < SEGMENTS; k++)
        {
            curacc[k] = acc[k];
            curSegLen[k] = segLenBase[k];
        }

        ei = -1;
        for (l = minl, curEnd = j, curex = ex, curex2 = ex2; l <= maxl && curEnd < dl; curEnd++, l++)
        {
            ///////////////////////////////////////
            // calculate the info for (i, l)
            // PAAs, mean, std
            ///////////////////////////////////////
            // cout << l << " " << ei << endl;
            d = data[curEnd];
            curex += d;
            curex2 += d * d;

            // gettimeofday(&time_start1, NULL);
            for (k = 0; k < SEGMENTS; k++)
            {
                if (idxOut[l - minl][k] != -1)
                    curacc[k] -= data[i + idxOut[l - minl][k]], curSegLen[k]--;
                if (idxIn[l - minl][k] != -1)
                    curacc[k] += data[i + idxIn[l - minl][k]], curSegLen[k]++;
                //  += idxIn[l - minl][k].size() - idxOut[l - minl][k].size();
            }
            // gettimeofday(&time_end1, NULL);
            // tss = time_start1.tv_sec;
            // tes = time_end1.tv_sec;
            // ts = (time_start1.tv_usec);
            // te = (time_end1.tv_usec);
            // diff = (tes - tss) + ((te - ts) / 1000000.0);
            // testTime += diff;

            mean = curex / l;
            std = curex2 / l;
            std = sqrt(std - mean * mean);
            //            cout << i << " " << l << " " << ei << endl;
            for (k = 0; k < SEGMENTS; k++)
            {
                PAAs[k] = (curacc[k] - mean * curSegLen[k]) / std / curSegLen[k];
            }
            /////////////////END///////////////////

            // cout << i << " " << l << " " << mean << " " << std << " ";
            // for (k = 0; k < SEGMENTS; k++)
            //     cout << PAAs[k] << " ";
            // for (k = 0; k < SEGMENTS; k++)
            //     cout << getSAXFromPAA(PAAs[k]) << " ";
            // cout << endl;

            // decide the envelope to update
            if ((l - minl) % gamma == 0)
            {
                ei++;
            }

            if (i % beta == 0 && (l - minl) % gamma == 0)
            {
                envs[ei].reset(i);
                envCnt++;
                envs[ei].minl = l;
                envs[ei].ex = curex - d;
                envs[ei].ex2 = curex2 - d * d;
            }

            envs[ei].cnt++;

            if (mean > envs[ei].meanMax)
                envs[ei].meanMax = mean;
            else if (mean < envs[ei].meanMin)
                envs[ei].meanMin = mean;
            if (std > envs[ei].stdMax)
                envs[ei].stdMax = std;
            else if (std < envs[ei].stdMin)
                envs[ei].stdMin = std;
            // envs[ei].meanMax = max(mean, envs[ei].meanMax);
            // envs[ei].meanMin = min(mean, envs[ei].meanMin);
            // envs[ei].stdMax = max(std, envs[ei].stdMax);
            // envs[ei].stdMin = min(std, envs[ei].stdMin);

            for (k = 0; k < SEGMENTS; k++)
            {
                if (PAAs[k] > envs[ei].PAAmax[k])
                    envs[ei].PAAmax[k] = PAAs[k];
                else if (PAAs[k] < envs[ei].PAAmin[k])
                    envs[ei].PAAmin[k] = PAAs[k];
                // envs[ei].PAAmax[k] = max(PAAs[k], envs[ei].PAAmax[k]);
                // envs[ei].PAAmin[k] = min(PAAs[k], envs[ei].PAAmin[k]);
                envs[ei].PAAkey[k] += PAAs[k];
            }
        }

        // write env infos file_position_typeo file per beta step or reaching the end
        if ((i + 1) % beta == 0 || i == dl - minl)
        {
            for (int tmpi = 0; tmpi < envCnt; tmpi++)
            {
                for (int k = 0; k < SEGMENTS; k++)
                {
                    envs[tmpi].PAAkey[k] /= envs[tmpi].cnt;
                }
            }
            writeEnvs(envCnt);
            envCnt = 0;
            // fwrite(envs.data(), sizeof(EnvInfo), envCnt, outfp);
        }

        ex += data[j] - data[i];
        ex2 += data[j] * data[j] - data[i] * data[i];
        for (k = 0; k < SEGMENTS; k++)
        {
            acc[k] += data[segRightIndexBase[k] + i] - data[segLeftIndexBase[k] + i];
        }
    }

    if (flush_cnt != 0)
    {
        flushAllEnv();
    }
    // cout << envTotalCnt << endl;
    fflush(outfp);
    fclose(outfp);

    free(envs);
    free(saxEnvs);

    free(startPos_flush);
    free(minLen_flush);

    // gettimeofday(&time_end, NULL);

    // tss = time_start.tv_sec;
    // tes = time_end.tv_sec;
    // ts = (time_start.tv_usec);
    // te = (time_end.tv_usec);
    // diff = (tes - tss) + ((te - ts) / 1000000.0);
    // totalTime = diff;
    // cout << testTime / totalTime << endl;
    cout << "Envelope num: " << envTotalCnt << endl;
}