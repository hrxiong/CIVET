//
//  sax.c
//  isaxlib
//
//  Created by Kostas Zoumpatianos on 3/10/12.
//  Copyright 2012 University of Trento. All rights reserved.
//
//#define DEBUG;
#include "config.h"
#include "globals.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
// #ifdef VALUES
// //	#include <values.h>
// #include <float.h>
// #endif

#include "sax/sax.h"
#include "sax/ts.h"
#include "sax/sax_breakpoints.h"

/** 
 This is used for converting to sax
 */
int compare(const void *a, const void *b)
{
    float * c = (float *) b - 1;
    if (*(float*)a>*(float*)c && *(float*)a<=*(float*)b) {
        //printf("Found %lf between %lf and %lf\n",*(float*)a,*(float*)c,*(float*)b);
        return 0;
    }
    else if (*(float*)a<=*(float*)c) {
        return -1;
    }
    else
    {
        return 1;
    }
}

/** 
 Calculate paa.
 */
enum response paa_from_ts (ts_type *ts_in, ts_type *paa_out, int segments, int ts_values_per_segment) {
    int s, i;
    for (s=0; s<segments; s++) {
        paa_out[s] = 0;
        for (i=0; i<ts_values_per_segment; i++) {
            paa_out[s] += ts_in[(s * ts_values_per_segment)+i];
        }
        paa_out[s] /= ts_values_per_segment;
    }
    return SUCCESS;
}


enum response sax_from_paa (ts_type *paa, sax_type *sax, int segments,
                            int cardinality, int bit_cardinality) {
    int offset = ((cardinality - 1) * (cardinality - 2)) / 2;
    //printf("FROM %lf TO %lf\n", sax_breakpoints[offset], sax_breakpoints[offset + cardinality - 2]);

    int si;
    for (si=0; si<segments; si++) {
        sax[si] = 0;

        // First object = sax_breakpoints[offset]
        // Last object = sax_breakpoints[offset + cardinality - 2]
        // Size of sub-array = cardinality - 1

        float *res = (float *) bsearch(&paa[si], &sax_breakpoints[offset], cardinality - 1,
                                       sizeof(ts_type), compare);
        if(res != NULL)
	  {
            //sax[si] = (int) (res -  &sax_breakpoints[offset]);
	    sax[si] = (sax_type) (res -  &sax_breakpoints[offset]);
	  }
        else if (paa[si] > 0)
	  {
            sax[si] = cardinality-1;
	  }
    }

    return SUCCESS;
}

/**
 This function converts a ts record into its sax representation.
 */
enum response sax_from_ts(ts_type *ts_in, sax_type *sax_out, int ts_values_per_segment,
                 int segments, int cardinality, int bit_cardinality)
{
    // Create PAA representation
    float * paa = malloc(sizeof(float) * segments);
    if(paa == NULL) {
        fprintf(stderr,"error: could not allocate memory for PAA representation.\n");
        return FAILURE;
    }

    int s, i;
    for (s=0; s<segments; s++) {
        paa[s] = 0;
        for (i=0; i<ts_values_per_segment; i++) {
            paa[s] += ts_in[(s * ts_values_per_segment)+i];
        }
        paa[s] /= ts_values_per_segment;
//#ifdef DEBUG
        //printf("%d: %lf\n", s, paa[s]);
//#endif
    }

    // Convert PAA to SAX
    // Note: Each cardinality has cardinality - 1 break points if c is cardinality
    //       the breakpoints can be found in the following array positions: 
    //       FROM (c - 1) * (c - 2) / 2 
    //       TO   (c - 1) * (c - 2) / 2 + c - 1
    int offset = ((cardinality - 1) * (cardinality - 2)) / 2;
    //printf("FROM %lf TO %lf\n", sax_breakpoints[offset], sax_breakpoints[offset + cardinality - 2]);

    int si;
    for (si=0; si<segments; si++) {
        sax_out[si] = 0;

        // First object = sax_breakpoints[offset]
        // Last object = sax_breakpoints[offset + cardinality - 2]
        // Size of sub-array = cardinality - 1

        float *res = (float *) bsearch(&paa[si], &sax_breakpoints[offset], cardinality - 1,
                                       sizeof(ts_type), compare);
        if(res != NULL)
	  {
            //sax_out[si] = (int) (res -  &sax_breakpoints[offset]);
	    sax_out[si] = (sax_type) (res -  &sax_breakpoints[offset]);
	  }
        else if (paa[si] > 0)
	  sax_out[si] = (sax_type) (cardinality-1);
    }

    //sax_print(sax_out, segments, cardinality);
    free(paa);
    return SUCCESS;
}


void printbin(unsigned long long n, int size) {
    char *b = malloc(sizeof(char) * (size+1));
    int i;

    for (i=0; i<size; i++) {
        b[i] = '0';
    }

    for (i=0; i<size; i++, n=n/2)
        if (n%2) b[size-1-i] = '1';

    b[size] = '\0';
    printf("%s\n", b);
    free(b);
}

void serial_printbin (unsigned long long n, int size) {
    char *b = malloc(sizeof(char) * (size+1));
    int i;

    for (i=0; i<size; i++) {
        b[i] = '0';
    }

    for (i=0; i<size; i++, n=n/2)
        if (n%2) b[size-1-i] = '1';

    b[size] = '\0';
    printf(" %s ", b);
}


/**
 This function prints a sax record.
 */
void sax_print(sax_type *sax, int segments, int cardinality)
{
    int i;
    for (i=0; i < segments; i++) {
        printf("%d:\t", i);
        printbin(sax[i], cardinality);
    }
    printf("\n");
}


ts_type minidist_paa_to_isax(ts_type  *paa, sax_type *sax,
                           sax_type *sax_cardinalities,
                           sax_type max_bit_cardinality,
                           sax_type max_cardinality,
                           int number_of_segments,
                           int min_val,
                           int max_val,
                           float ratio_sqrt)
{

    ts_type distance = 0;
    // TODO: Store offset in index settings. and pass index settings as parameter.

    int offset = ((max_cardinality - 1) * (max_cardinality - 2)) / 2;

    // For each sax record find the break point
    int i;
    for (i=0; i<number_of_segments; i++) {

        sax_type c_c = sax_cardinalities[i];
        sax_type c_m = max_bit_cardinality;
        sax_type v = sax[i];


        sax_type region_lower = v << (c_m - c_c);
        sax_type region_upper = (~((int)FLT_MAX << (c_m - c_c)) | region_lower);


	/*
        int region_lower = v << (c_m - c_c);
        int region_upper = (~((int)MAXFLOAT << (c_m - c_c)) | region_lower);
	*/

        ts_type breakpoint_lower = 0; // <-- TODO: calculate breakpoints.
        ts_type breakpoint_upper = 0; // <-- - || -


        if (region_lower == 0) {
            breakpoint_lower = min_val;
        }
        else
        {
            breakpoint_lower = sax_breakpoints[offset + region_lower - 1];
        }
        if (region_upper == max_cardinality - 1) {
            breakpoint_upper = max_val;
        }
        else
        {
            breakpoint_upper = sax_breakpoints[offset + region_upper];
        }
        //printf("FROM: \n");
        //sax_print(&region_lower, 1, c_m);
        //printf("TO: \n");
        //sax_print(&region_upper, 1, c_m);
        //printf("\n%d.%d is from %d to %d, %lf - %lf\n", v, c_c, region_lower, region_upper,
        //       breakpoint_lower, breakpoint_upper);

        if (breakpoint_lower > paa[i]) {
            distance += pow(breakpoint_lower - paa[i], 2);
        }
        else if(breakpoint_upper < paa[i]) {
            distance += pow(breakpoint_upper - paa[i], 2);
        }
//        else {
//            printf("%lf is between: %lf and %lf\n", paa[i], breakpoint_lower, breakpoint_upper);
//        }
    }

    //distance = ratio_sqrt * sqrtf(distance);  
    distance = ratio_sqrt * distance;
    return distance;
}


ts_type minidist_paa_to_isax_raw(ts_type *paa, sax_type *sax,
                           sax_type *sax_cardinalities,
                           sax_type max_bit_cardinality,
                           int max_cardinality,
                           int number_of_segments,
                           int min_val,
                           int max_val,
                           ts_type ratio_sqrt)
{

    float distance = 0;
    // TODO: Store offset in index settings. and pass index settings as parameter.

    int offset = ((max_cardinality - 1) * (max_cardinality - 2)) / 2;

    // For each sax record find the break point
    int i;
    for (i=0; i<number_of_segments; i++) {

        sax_type c_c = sax_cardinalities[i];
        sax_type c_m = max_bit_cardinality;
        sax_type v = sax[i];
        //sax_print(&v, 1, c_m);

        sax_type region_lower = (v >> (c_m - c_c)) <<  (c_m - c_c);
        sax_type region_upper = (~((int)FLT_MAX << (c_m - c_c)) | region_lower);
		//printf("[%d, %d] %d -- %d\n", sax[i], c_c, region_lower, region_upper);

        float breakpoint_lower = 0; // <-- TODO: calculate breakpoints.
        float breakpoint_upper = 0; // <-- - || -


        if (region_lower == 0) {
            breakpoint_lower = min_val;
        }
        else
        {
            breakpoint_lower = sax_breakpoints[offset + region_lower - 1];
        }
        if (region_upper == max_cardinality - 1) {
            breakpoint_upper = max_val;
        }
        else
        {
            breakpoint_upper = sax_breakpoints[offset + region_upper];
        }

        //printf("\n%d.%d is from %d to %d, %lf - %lf\n", v, c_c, region_lower, region_upper,
        //       breakpoint_lower, breakpoint_upper);

        //printf("FROM: \n");
        //sax_print(&region_lower, 1, c_m);
        //printf("TO: \n");
        //sax_print(&region_upper, 1, c_m);

		//printf ("\n---------\n");

        if (breakpoint_lower > paa[i]) {
            distance += pow(breakpoint_lower - paa[i], 2);
        }
        else if(breakpoint_upper < paa[i]) {
            distance += pow(breakpoint_upper - paa[i], 2);
        }
//        else {
//            printf("%lf is between: %lf and %lf\n", paa[i], breakpoint_lower, breakpoint_upper);
//        }
    }

    //distance = ratio_sqrt * sqrtf(distance);
    distance = ratio_sqrt * distance;
    return distance;
}
