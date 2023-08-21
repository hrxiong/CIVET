#ifndef PROJECT_UTILS_UTILS_H
#define PROJECT_UTILS_UTILS_H

extern "C"
{
#include "globals.h"
}

#define SEGMENTS 8
// #define SEGMENTS 16

#define INF 1e20

#define BINSUFFIX ".bin"
#define METABINSUFFIX ".bin-META"

#define REFLUSHNUM 100000

#define MAX_VISIT_LEAF_SIZE 5

// typedef double ts_type;
// typedef long long file_position_type;
// typedef unsigned short sax_type;

// typedef double ts_type;
// typedef int file_position_type;
// typedef unsigned short sax_type;

extern ts_type saxBreakPoints[255];

struct EnvInfoPAA
{
    file_position_type startIndex, minl, cnt;
    ts_type ex, ex2, meanMin, meanMax, stdMin, stdMax;
    ts_type PAAmax[SEGMENTS], PAAmin[SEGMENTS], PAAkey[SEGMENTS];

    void reset(file_position_type start);
};

struct EnvInfoSAX
{
    file_position_type startIndex, minl;
    ts_type ex, ex2, meanMin, meanMax, stdMin, stdMax;
    sax_type SAXmax[SEGMENTS], SAXmin[SEGMENTS], SAXkey[SEGMENTS];
    sax_type invSAX[2*SEGMENTS];

    // void reset(file_position_type start);
    void setByPAAEnv(struct EnvInfoPAA &env);
};

class US_ED
{
public:
    ts_type bsf;

    US_ED() = default;

    virtual void initialQuery(ts_type *query, file_position_type ql) = 0;
    virtual void initialData(ts_type *data, file_position_type dLength) = 0;
    virtual void run() = 0;

    inline ts_type getBsf() { return bsf; }
};

sax_type getSAXFromPAA(ts_type paa);

ts_type getSAXLowerBP(sax_type sax);

ts_type getSAXUpperBP(sax_type sax);

void norm(ts_type *data, file_position_type l);

/// Data structure (circular array) for finding minimum and maximum for LB_Keogh envolop
typedef struct lb_deque
{   int *dq;
    int size,capacity;
    int f,r;
} lb_deque;
void init(lb_deque *d, int capacity);
void push_back(struct lb_deque *d, int v);
void pop_back(struct lb_deque *d);
int front(struct lb_deque *d);
int back(struct lb_deque *d);
int empty(struct lb_deque *d);
void destroy(lb_deque *d);
void lower_upper_lemire(ts_type *t, int len, int r, ts_type *l, ts_type *u);


#endif // PROJECT_UTILS_UTILS_H
