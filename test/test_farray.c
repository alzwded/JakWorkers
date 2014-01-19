#include "../JakWorkers.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define N (10 * 1024 * 1024)
float a[N];

void generateArray()
{
    time_t start, stop;
    size_t i,j = 0;
    printf("starting to generate array\n");
    start = time(NULL);
    for(i = 0; i < N; ++i) {
        a[i] = (float)i;
    }
    stop = time(NULL);
    printf("array generation took %gs\n", difftime(stop, start));
}

typedef struct {
    size_t start, finish;
} data_t;

void compute(void* data)
{
    printf("job started\n");
    fflush(stdout);
    data_t* sf = (data_t*)data;
    size_t i, j;
    for(j = 0; j < 5; ++j) {
        for(i = sf->start; i < sf->finish; ++i) {
            a[i] *= a[i];
            a[i] = sqrt(a[i] + 36) * sin(a[i]);
        }
    }
    free(sf);
    printf("job done\n");
    fflush(stdout);
}

int main(int argc, char* argv[])
{
    time_t startMulti, stopMulti, stopSingle, startSingle;
    double tMulti, tSingle;
    size_t i;
    jw_config_t config = JW_CONFIG_INITIALIZER;
    data_t* singleData;

    generateArray();

    startMulti = time(NULL);

    jw_init(config);

    for(i = 0; i < 10; ++i) {
        data_t* data = (data_t*)malloc(sizeof(data_t));
        data->start = i * (N / 10);
        data->finish = (i + 1) * (N / 10);
        jw_add_job(&compute, data);
    }

    jw_main();

    stopMulti = time(NULL);
    tMulti = difftime(stopMulti, startMulti);

    printf("==========================\n");
    printf("multi-thread test: %gs\n", tMulti);
    printf("==========================\n");

    generateArray();

    startSingle = time(NULL);

    singleData = (data_t*)malloc(sizeof(data_t));
    singleData->start = 0;
    singleData->finish = N;
    compute(singleData);

    stopSingle = time(NULL);
    tSingle = difftime(stopSingle, startSingle);

    printf("==========================\n");
    printf("single-thread test: %gs\n", tSingle);
    printf("==========================\n");

    printf("multi vs. single: %gs, increase: %g\n", tSingle - tMulti, tSingle / tMulti);
    printf("==========================\n");

    return 0;
}
