#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include "../JakWorkers.h"

volatile sig_atomic_t sigRec = 0;
volatile sig_atomic_t sigX = 0;

void handle(int signum)
{
    sigRec++;
}

void job(void* data)
{
    static int n = 0;
    int myN = ++n;
    printf("job %d started\n", myN); fflush(stdout);
    sleep(10);
    printf("job %d ended\n", myN); fflush(stdout);
}

void loop(void* _not_used)
{
    static int n = 0;
    for(;!sigX;) {
        if(sigRec > 0) {
            sigRec--;
            jw_add_job(&job, NULL);
        } else {
            sleep(1);
        }
    }
}

void hsigusr1(int signum)
{
    sigX++;
    jw_exit(0);
}

int main()
{
    jw_config_t config = JW_CONFIG_INITIALIZER;
    config.EXIT_WHEN_ALL_JOBS_COMPLETE = 0;
    config.numWorkers = 5;
    jw_init(config);
    jw_add_job(&loop, NULL);
    signal(SIGINT, handle);
    signal(SIGUSR1, hsigusr1);
    jw_main();
}
