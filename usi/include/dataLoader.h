//
// Created by hrxiong on 17/3/22.
//

#ifndef PROJECT_DATALOADER_H
#define PROJECT_DATALOADER_H

#include "utils.h"

#include <string>

//

class BinDataLoader
{
    FILE *infp;

public:
    file_position_type totalNum, curNum;
    file_position_type *timeSeriesLen;

    ts_type *timeSeries;
    file_position_type dl;

    BinDataLoader();
    BinDataLoader(std::string filename);
    ~BinDataLoader();

    bool getNext();
    void reset();
};

class EnvelopeLoader
{
public:
    FILE *infp;
    EnvInfoSAX *envs;
    file_position_type envNum, curIdx;

    EnvelopeLoader();
    EnvelopeLoader(std::string filename);
    ~EnvelopeLoader();

    EnvInfoSAX *getNext();
    void reset();
};

// input: time serieses splited by newline, data in a time series splited by space
// output(binary file) ts1, ts2, ..., tn, len1, len2, ..., count
void convert2Bin(std::string filename);

// void test(std::string filename);

#endif // PROJECT_UTILS_UTILS_H
