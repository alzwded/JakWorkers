// Copyright (c) 2014, Vlad Mesco
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
// 
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright notice, this
//   list of conditions and the following disclaimer in the documentation and/or
//   other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "JakWorkers.h"
#include <pthreads.h>
#include <semaphore.h>
#include <stdlib.h>
#include <signal.h>

// typedefs
typedef struct {
    jw_job_func_t func;
    void* data;
} job_t;

typedef struct queue_s {
    struct queue_s* next;
    job_t job;
} queue_t;

typedef struct {
    pthread_t tid;
    job_t* job;
    pthread_cond_t job_cond;
    pthread_mutex_t job_condMutex;
} worker_t;

// module variables
static queue_t* jw_jobQueue = NULL;
static pthread_mutex_t jw_jobQueue_lock;
static pthread_cond_t jw_jobAdded;

static worker_t* jw_workers = NULL;
static sem_t jw_workersSem;

static jw_config_t jw_config;
static sig_atomic_t jw_exit_called;
static short jw_exit_code;

// internal functions
static void* jw_worker(void* data)
{
    job_t* myJob = (job_t*)data;

    pthread_mutex_lock(myJob->job_condMutex);
    for(;;) {
        while(!myJob->job->func)
            pthread_cond_wait(&myJob->job_cond, &myJob->job_condMutex);
        pthread_mutex_unlock(myJob->job_condMutex);

        myJob->job->func(myJob->job->data);
        myJob->job->func = NULL;
        myJob->job->data = NULL;

        pthread_mutex_lock(myJob->job_condMutex);
        sem_post(&jw_workersSem);
    }

    return NULL;
}

static void jw_cleanup()
{
    queue_t* q = jw_jobQueue;
    size_t i;

    while(q) {
        queue_t nq = q->next;
        free(q);
        q = nq;
    }

    for(i = 0; i < jw_config.numWorkers; ++i) {
        pthread_kill(jw_workers[i].tid, SIGKILL);
        free(jw_workers[i].job);
        pthread_mutex_destroy(&jw_workers[i].job_condMutex);
        pthread_cond_destroy(&jw_workers[i].job_cond);
    }
    free(jw_workers);

    pthread_mutex_destroy(&jw_jobQueue_lock);
    pthread_cond_destroy(&jw_jobAdded);

    sem_destroy(&jw_workersSem);


}

// entry points
short jw_main()
{
    for(;!jw_exit_called;) {
        job_t job;
        queue_t q;
        size_t i;

        sem_wait(&jw_workersSem);

        if(jw_exit_called) break;

        pthread_mutex_lock(&jw_jobQueue_lock);
        if(!jw_config.EXIT_WHEN_ALL_JOBS_COMPLETE) {
            while(!jw_jobQueue)
                pthread_cond_wait(&jw_jobAdded, &jw_jobQueue);
            if(jw_exit_called) break;
        } else {
            if(!jw_jobQueue) {
                jw_exit(0);
                pthread_mutex_unlock(&jw_jobQueue_lock);
                break;
            }
        }
        job = jw_jobQueue->job;
        q = jw_jobQueue;
        jw_jobQueue = jw_jobQueue->next;
        pthread_mutex_unlock(&jw_jobQueue_lock);

        free(q);

        for(i = 0; i < jw_config.numWorkers; ++i) {
            if(!jw_workers[i].job->func) {
                pthread_mutex_lock(&jw_workers[i].job_condMutex);
                *jw_workers[i].job = job;
                pthread_cond_signal(&jw_workers[i].job_cond);
                pthread_mutex_unlock(&jw_workers[i].job_condMutex);
                break;
            }
        }
    }

    while(jw_exit_called < 2) pthread_yield();
    pthread_mutex_lock(jw_jobQueue_lock);
    jw_cleanup();

    return jw_exit_code;
}

short jw_init(jw_config_t const config)
{
    size_t i;

    jw_exit_called = 0;

    jw_config = config;
    jw_jobQueue = NULL;
    jw_workers = (worker_t*)calloc(sizeof(worker_t) * config.numWorkers);

    sem_init(&jw_workersSem, 0, config.numWorkers);
    pthread_mutex_init(&jw_jobQueue_lock, NULL);
    pthread_mutex_init(&jw_jobAddedMutex, NULL);
    pthread_cond_init(&jw_jobAdded, NULL);

    for(i = 0; i < config.numWorkers; ++i) {
        jw_workers[i].job = (job_t*)malloc(sizeof(job_t));
        pthread_mutex_init(&jw_workers[i].job_condMutex, NULL);
        pthread_cond_init(&jw_workers[i].job_cond, NULL);
        pthread_create(&jw_workers[i].tid,
                NULL,
                &jw_worker,
                jw_workers[i].job);
    }
}

#define ADD_TO_QUEUE(Q) do{ \
    Q = (queue_t*)malloc(sizeof(queue_t)); \
    Q->next = NULL; \
    Q->job.func = func; \
    Q->job.data = data; \
}while(0)
short jw_add_job(jw_job_func_t func, void* data)
{
    int status = 0;
    if(!func) abort();
    status = pthread_mutex_lock(&jw_jobQueue_lock);
    if(status) {
        pthread_perror("The JW framework is not running", status);
        abort();
    }
    if(!jw_jobQueue) {
        ADD_TO_QUEUE(jw_jobQueue);
    } else {
        queue_t* n = jw_jobQueue;
        while(n->next) n = n->next;
        ADD_TO_QUEUE(n->next);
    }
    pthread_cond_signal(&jw_jobAdded);
    pthread_mutex_unlock(&jw_jobQueue_lock);
}

short jw_exit(short code)
{
    jw_exit_code = code;

    jw_exit_called++;

    // wake up main thread if it was sleeping
    // on account of all workers being busy
    sem_post(&jw_workersSem); 
    // or because it had no jobs
    pthread_mutex_lock(&jw_jobQueue_lock);
    pthread_cond_signal(&jw_jobAdded);
    pthread_mutex_unlock(&jw_jobQueue_lock);

    jw_exit_called++;
}
