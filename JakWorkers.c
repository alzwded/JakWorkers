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
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
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
    sig_atomic_t init;
} worker_t;

// module variables
// ================
// the job queue
// jobs are added here via jw_add_task and consumed in jw_main by 
// dispatching them to workers
static queue_t* jw_jobQueue = NULL;
static pthread_mutex_t jw_jobQueue_lock;
static pthread_cond_t jw_jobAdded;

// the list of workers
// TODO make it a worker_t** to allow resizing
static worker_t* jw_workers = NULL;
static sem_t jw_workersSem;

static jw_config_t jw_config;
static sig_atomic_t jw_exit_called;
static int jw_exit_code;

// internal functions
static void* jw_worker(void* data)
{
    // retrieve the data I'll be working with
    worker_t* myJob = ((worker_t*)data);

    // lock immediately, because I don't want jw_main to give us work
    // before we're ready
    pthread_mutex_lock(&myJob->job_condMutex);
    myJob->init++;
    for(;;) {
        // if I got nothing...
        while(!myJob->job->func && !jw_exit_called) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            if(ts.tv_nsec + 5000000ul < ts.tv_nsec) {
                ts.tv_nsec += 50000000ul;
                ts.tv_sec++;
            } else {
                ts.tv_nsec += 50000000ul;
            }
            // sleep
            pthread_cond_timedwait(&myJob->job_cond, &myJob->job_condMutex, &ts);
        }
        // I've received some work!
        pthread_mutex_unlock(&myJob->job_condMutex);
        if(jw_exit_called) {
            pthread_mutex_destroy(&myJob->job_condMutex);
            pthread_cond_destroy(&myJob->job_cond);
            pthread_exit(0);
        }

        // I will do my job
        myJob->job->func(myJob->job->data);

        // Lock my data because I don't want jw_main to give me work
        // before I'm ready to accept it
        pthread_mutex_lock(&myJob->job_condMutex);
        // Now that I'm done, I can get forget about it. That's behind me
        myJob->job->func = NULL;
        myJob->job->data = NULL;
        // Notify jw_main that I'm ready to do more work
        sem_post(&jw_workersSem);
    }

    // never reached
    return NULL;
}

static void jw_cleanup()
{
    queue_t* q;
    size_t i;

    // jw_exit might be called while jw_main is waiting for new jobs to
    // appear, and jw_exit needs time to handle that case. This is why
    // the jw_exit_called<2 condition is here.
    while(jw_exit_called < 2) pthread_yield();
    // lock the queue. No new jobs can be assigned
    pthread_mutex_lock(&jw_jobQueue_lock);

    // delete everything
    q = jw_jobQueue;

    while(q) {
        queue_t* nq = q->next;
        free(q);
        q = nq;
    }

    // destroy workers
    for(i = 0; i < jw_config.numWorkers; ++i) {
        pthread_join(jw_workers[i].tid, NULL);
        free(jw_workers[i].job);
    }
    free(jw_workers);

    // clear remaining mutexes, cond variables and semaphores
    pthread_mutex_unlock(&jw_jobQueue_lock);
    pthread_mutex_destroy(&jw_jobQueue_lock);
    pthread_cond_destroy(&jw_jobAdded);

    sem_destroy(&jw_workersSem);


}

// entry points
int jw_main()
{
    for(;!jw_exit_called;) {
        job_t job;
        queue_t* q;
        size_t i;

        // if exit was called, break out of the loop
        if(jw_exit_called) break;

        // lock the job queue because I don't want anyone adding stuff to
        // it while I'm checking if anything was added or consuming a job...
        pthread_mutex_lock(&jw_jobQueue_lock);
        // if I am NOT supposed to exit when all jobs are complete...
        if(!jw_config.EXIT_WHEN_ALL_JOBS_COMPLETE) {
            // check if I have any jobs. If exit was not called, sleep
            while(!jw_jobQueue && !jw_exit_called)
                pthread_cond_wait(&jw_jobAdded, &jw_jobQueue_lock);
            // if exit was called, break out of the loop
            if(jw_exit_called) {
                pthread_mutex_unlock(&jw_jobQueue_lock);
                break;
            }
        // I'm supposed to exit if I have no jobs left
        } else {
            // yup, no jobs
            if(!jw_jobQueue) {
                int value = -1;
                sem_getvalue(&jw_workersSem, &value);
                // No more jobs, yey!
                if(value >= jw_config.numWorkers) {
                    pthread_mutex_unlock(&jw_jobQueue_lock);
                    jw_exit_code = 0;
                    jw_exit_called = 2;
                    break;
                // Oh no, there are still active jobs!
                } else {
                    // don't pthread_cond_wait here because there
                    // might be no one to add new tasks ever
                    pthread_mutex_unlock(&jw_jobQueue_lock);
                    // let workers do their work, then continue from the top
                    pthread_yield();
                    continue;
                }
            }
        }
        // Exit was called, break out of the loop...
        if(jw_exit_called) {
            pthread_mutex_unlock(&jw_jobQueue_lock);
            break;
        }
        // take the next job
        job = jw_jobQueue->job;
        q = jw_jobQueue;
        jw_jobQueue = jw_jobQueue->next;
        // release the job queue, feel free to add new jobs now...
        // I'm not gonna check it again until later
        pthread_mutex_unlock(&jw_jobQueue_lock);

        free(q);

        // Wait if there are no free workers
        sem_wait(&jw_workersSem);

        // Find which worker is free
        for(i = 0; i < jw_config.numWorkers; ++i) {
            if(!jw_workers[i].job->func) {
                // lock his data, I will not give him a task
                pthread_mutex_lock(&jw_workers[i].job_condMutex);
                *jw_workers[i].job = job;
                // tell it I'm done. It's free to do its thing now
                pthread_cond_signal(&jw_workers[i].job_cond);
                pthread_mutex_unlock(&jw_workers[i].job_condMutex);
                break;
            }
        }
    }

    // exit was called, clean up ALL data (jobqueue, workers, other)
    jw_cleanup();

    return jw_exit_code;
}

int jw_init(jw_config_t const config)
{
    size_t i;

    jw_exit_called = 0;

    jw_config = config;
    jw_jobQueue = NULL;
    jw_workers = (worker_t*)calloc(config.numWorkers, sizeof(worker_t));

    sem_init(&jw_workersSem, 0, config.numWorkers);
    pthread_mutex_init(&jw_jobQueue_lock, NULL);
    pthread_cond_init(&jw_jobAdded, NULL);

    for(i = 0; i < config.numWorkers; ++i) {
        jw_workers[i].job = (job_t*)malloc(sizeof(job_t));
        jw_workers[i].job->func = NULL;
        jw_workers[i].job->data = NULL;
        jw_workers[i].init = 0;
        pthread_mutex_init(&jw_workers[i].job_condMutex, NULL);
        pthread_cond_init(&jw_workers[i].job_cond, NULL);
        pthread_create(&jw_workers[i].tid,
                NULL,
                &jw_worker,
                &jw_workers[i]);
        while(!jw_workers[i].init) pthread_yield();
    }
}

#define ADD_TO_QUEUE(Q) do{ \
    Q = (queue_t*)malloc(sizeof(queue_t)); \
    Q->next = NULL; \
    Q->job.func = func; \
    Q->job.data = data; \
}while(0)
int jw_add_job(jw_job_func_t func, void* data)
{
    int status = 0;
    // try to play nice...
    if(!func) abort();
    // lock the job queue. I don't want jw_main to mess with it while
    // I'm messing with it...
    status = pthread_mutex_lock(&jw_jobQueue_lock);
    // Oups... I guess I was waiting for nothing
    if(status) {
        fprintf(stderr, "The JW framework is not running: %d\n", status);
        return 1;
    }
    // Uh... dafuq? Might as well return with an error....
    if(jw_exit_called) {
        fprintf(stderr, "The JW framework was closed while adding this task: %d\n", status);
        return 1;
    }
    // Hurray, I can add my job to the queue now!
    if(!jw_jobQueue) {
        ADD_TO_QUEUE(jw_jobQueue);
    } else {
        queue_t* n = jw_jobQueue;
        while(n->next) n = n->next;
        ADD_TO_QUEUE(n->next);
    }
    // Notify jw_main it can start processing tasks now, it might have
    // fallen asleep since the last time anyone's talked to it...
    pthread_cond_signal(&jw_jobAdded);
    pthread_mutex_unlock(&jw_jobQueue_lock);
    return 0;
}

int jw_exit(int code)
{
    // first phase of exit. This is meant to make jw_main aware we are now
    // in the process of bringing the system down
    jw_exit_called++;

    // set the return code
    jw_exit_code = code;

    // wake up main thread if it was sleeping
    // on account of all workers being busy
    sem_post(&jw_workersSem); 
    // or because it had no jobs
    pthread_mutex_lock(&jw_jobQueue_lock);
    pthread_cond_signal(&jw_jobAdded);
    pthread_mutex_unlock(&jw_jobQueue_lock);

    // The exit procedure is finished. Tell jw_main it can clean up now
    jw_exit_called++;
}
