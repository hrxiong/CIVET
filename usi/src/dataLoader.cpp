#include "dataLoader.h"

#include <fstream>
#include <sstream>
#include <stdio.h>
#include <vector>
#include <iostream>

using namespace std;

void convert2Bin(string filename)
{
    ts_type d;
    cout << filename << endl;
    ifstream infs(filename);

    string outputFilename = filename + BINSUFFIX;
    string metaOutputFilename = filename + METABINSUFFIX;
    FILE *outfp = fopen(outputFilename.c_str(), "wb");
    FILE *metaOutfp = fopen(metaOutputFilename.c_str(), "wb");

    string line;
    vector<ts_type> timeSeries;
    vector<file_position_type> timeSeriesLen;
    cout << "Start converting data from " + filename << endl;

    ts_type tmp;
    file_position_type cnt = 0;

    // while(infs >> tmp){
    //     cnt++;
    //     fwrite(&tmp, sizeof(ts_type), 1, outfp);
    // }
    // file_position_type totalNum = 1;
    // fwrite(&totalNum, sizeof(totalNum), 1, metaOutfp);
    // fwrite(&cnt, sizeof(cnt), 1, metaOutfp);

    while (getline(infs, line))
    {
        stringstream ss;
        ss << line;

        while (ss >> d)
            timeSeries.push_back(d);
        timeSeriesLen.push_back(timeSeries.size());

        fwrite(timeSeries.data(), sizeof(ts_type), timeSeries.size(), outfp);

        timeSeries.clear();
    }

    file_position_type totalNum = timeSeriesLen.size();
    fwrite(&totalNum, sizeof(totalNum), 1, metaOutfp);
    fwrite(timeSeriesLen.data(), sizeof(file_position_type), timeSeriesLen.size(), metaOutfp);

    infs.close();
    fclose(outfp);
    fclose(metaOutfp);
}

BinDataLoader::BinDataLoader() = default;

BinDataLoader::BinDataLoader(std::string filename)
{
    string inFilename = filename + BINSUFFIX;
    string metaInFilename = filename + METABINSUFFIX;

    FILE *metaInfp = fopen(metaInFilename.c_str(), "rb");
    infp = fopen(inFilename.c_str(), "rb");

    curNum = 0;
    dl = 0;
    timeSeries = NULL;

    fread(&totalNum, sizeof(totalNum), 1, metaInfp);
    timeSeriesLen = (file_position_type *)malloc(sizeof(file_position_type) * totalNum);
    fread(timeSeriesLen, sizeof(file_position_type), totalNum, metaInfp);

    fclose(metaInfp);
}

BinDataLoader::~BinDataLoader()
{
    if (timeSeriesLen)
    {
        free(timeSeriesLen);
        timeSeriesLen = NULL;
    }
    if (timeSeries)
    {
        free(timeSeries);
        timeSeries = NULL;
    }
    fclose(infp);
}

bool BinDataLoader::getNext()
{
    if (timeSeries)
    {
        free(timeSeries);
        timeSeries = NULL;
    }

    if (curNum >= totalNum)
    {
        return false;
    }

    dl = timeSeriesLen[curNum++];
    timeSeries = (ts_type *)malloc(sizeof(ts_type) * dl);
    fread(timeSeries, sizeof(ts_type), dl, infp);

    return true;
}

void BinDataLoader::reset()
{
    if (timeSeries)
    {
        free(timeSeries);
        timeSeries = NULL;
    }
    curNum = 0;
    dl = 0;

    fseek(infp, 0, SEEK_SET);
}

EnvelopeLoader::EnvelopeLoader() = default;

EnvelopeLoader::EnvelopeLoader(string filename)
{
    infp = fopen(filename.c_str(), "rb");

    fseek(infp, 0, SEEK_END);
    long long fileLength = ftell(infp);
    envNum = fileLength / sizeof(EnvInfoSAX);

    fseek(infp, 0, SEEK_SET);

    envs = (EnvInfoSAX *)malloc(fileLength);
    fread(envs, sizeof(EnvInfoSAX), envNum, infp);

    reset();
}

EnvelopeLoader::~EnvelopeLoader()
{
    free(envs);
}

void EnvelopeLoader::reset()
{
    curIdx = 0;
}

EnvInfoSAX *EnvelopeLoader::getNext()
{
    EnvInfoSAX *ret = NULL;
    if (curIdx >= envNum)
        return ret;
    ret = envs + curIdx;
    curIdx++;
    return ret;
}