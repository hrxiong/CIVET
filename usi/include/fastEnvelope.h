//
// Created by hrxiong on 30/5/22.
//

#include "utils.h"

#include <string>
#include <vector>

class envBuilder
{
    int segLenBase[SEGMENTS], segLeftIndexBase[SEGMENTS], segRightIndexBase[SEGMENTS], curSegLen[SEGMENTS];
    ts_type acc[SEGMENTS], curacc[SEGMENTS];
    int window_size, flush_limit, flush_cnt;
    unsigned long long curEnv;

    int envTotalCnt;

    EnvInfoSAX* saxEnvs;
    EnvInfoPAA* envs;

    sax_type saxMin_flush[SEGMENTS], saxMax_flush[SEGMENTS];
    int saxMean_flush[SEGMENTS];
    file_position_type *startPos_flush;
    int *minLen_flush;

    FILE *outfp;

    int minl, maxl, beta, gamma;

public:
    envBuilder();
    envBuilder(std::string filename, int minl, int maxl, int beta, int gamma);

    // ~envBuilder();

    void getBulkEnvelopeImproved(ts_type *data, file_position_type dl, int window_size, int flush_limit);
    void prepare(ts_type *data, int minl, int maxl);
    void writeEnvs(int envCnt);

    void flushAllEnv();
};