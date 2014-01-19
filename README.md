JakWorkers
==========

My very own thread pool implementation.

Use cases
=========

TODO

* continuation
* delayed execution

The checkbox thing means it has one (or more) test case(s).

* [ ] Data parallelization (e.g. create an array of 1M floats, compute the squares on 4 workers) -> EXIT_WHEN_ALL_JOBS_COMPLETE = 0
* [ ] Performing separate tasks which don't inter-block themselves -> EXIT_WHEN_ALL_JOBS_COMPLETE = 1
* [ ] Continuation (TBD) (also, TODO)
* [ ] Delayed execution (TBD) (also, TODO)

Continuation
============

Yeah... the code you put in a worker should not, in turn, block, or you might bring the entire framework to a grinding halt. You should instead try to block, and if you can't, add a new job that tries to block and continues where you left off.

TODO In the future, I might add a flag on a job that might be blocking which will mean that the framework will try to keep at least one worker which undergoes only jobs which promise to never block. This way, there will be at least one worker that will never block, which means stuff will get done, which may mean that the other blocked threads will be able to continue where they left off assuming no dead-locks.

If you create jobs which inter-lock themselves on mutexes and expect the dispatcher to somehow magically guess what order they need to be run in in order to avoid dead-locks, that's your problem. This framework is not necessarily designed to perform tasks which are SUPER interdependent. Also, if you manage to have an application which requires inter-locking jobs, it means either a) you're doing it wrong, or b) you're doing it right, but what you need is something more complex than JakWorkers.

Caller
======

```C
int sigReceived = 0;
void SIGINT_handler(int signum) {
    sigReceived++;
}
someFunc {
    // ...
    waitpit blabla...
    while(sigReceived) {
        sigReceived--;
        struct job_t job = { &my_code, &my_data };
        jw_add_job(job);
    }
    // ...
}
...
jw_main(3);
```

Callee
======

```C
void my_code(void* data) {
    int* datadata = (int*)data;
    do_stuff(data[0], &data[1]);
}
```

Framework
=========

```C
// the main job queue
queue_t jw_jobQueue;
pthread_mutex_t jw_jobQueue_lock;
pthread_cond_t jw_jobAdded; // set to 0 when queue is empty,
                            // set to 1 when new job is added
pthread_mutext_t jw_jobAddedMutext;
sem_t jw_queueSem;  // starts off at numthreads, fw starts decrementing it
                    // threads return it when their job is done
                    // framework blocks if empty (i.e. worker pool is full)

// add a new job
// blocking if someone else is using the jobQueue
// wakes up the framework if it was idle
short jw_add_job(struct job_t job); // thread safe

// the workers
struct {
    job_t job;
    pthread_cond_t job_cond; // thread waits for 0->1 transition to continue
                             // used to give a new job to a worker
    pthread_mutex_t job_condMutex;
} jw_workers[];

jw_exit(int retCode); // clean up resources, call jw_exit_ret_code
                      // which can be a stub;

jw_main(int const numWorkers)
{
    init jw_queueSem to numWorkers;

    // init threads
    for(int i = 0; i < numWorkers; ++i) {
        // ...
        pthread_create(&fw_worker, &jw_workers[i]);
    }

    for(;;) {
        sem_wait(jw_queueSem);

        lock jw_jobQueue;
        if(!EXIT_WHEN_ALL_JOBS_COMPLETE)
            while(jw_jobQueue is empty) pthread_cond_wait(jw_jobAdded);
        else {
            jw_exit(0);
            return;
        }
        take msg from jw_jobQueue;
        release jw_jobQueue;

        find free thread => t;
        pthread_mutex_lock(t->job_condMutex);
        pthread_cond_signal(t->job_cond);
        pthread_mutex_unlock(t->job_condMutex);
    }
}

void* jw_worker(void* data)
{
    job_t* myJob = (job_t*)data;
    pthread_mutex_lock(myJob->job_cond_mutex); // don't let the fw assign me
                                           // anything until I wait or
                                           // or it has finished assigning
                                           // me a task
    for(;;) {
        // wait if I have no job
        while(!myJob->job->func) pthread_cond_wait(myJob->job_cond);
        pthread_mutex_unlock(myJob->job_cond_mutex);

        // perform job
        myJob->job->func(myJob->job->data);
        // notify that I have nothing to do
        myJob->job->func = NULL;

        // lock the condition mutex
        pthread_mutex_lock(myJob->job_condMutex);
        // THEN notify the fw that I am done
        sem_post(jw_queueSem);
        // if the framework finds me now, it has to wait until I wait
        // on my condition variable for it to assign me a task
    }
}

void jw_add_job(job_t job)
{
    pthread_mutex_lock(jw_jobQueue);
    add job to jw_jobQueue;
    // signal the main thread if it was waiting for a new job
    pthread_cond_signal(jw_jobAdded);
    pthread_mutex_unlock(jw_jobQueue);
}
```

Blocking situations
===================

| where | when | action required to esccape |
|-------|------|----------------------------|
| `jw_job_add` | other call to `jw_job_add`, framework's currently dispatching a job | wait |
| `jw_main` | `sem_take(jw_queueSem)` | a worker returns itself to the pool |
| `jw_main` | `lock jw_jobQueue` | calls to add_job end; wait |
| `jw_main` | queue is empty && !EXIT_WHEN_ALL_JOBS_COMPLETE | call to add_job |
| `jw_worker` | `pthread_cond_wait` | fw dispatches a job |
