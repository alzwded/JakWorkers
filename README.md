JakWorkers
==========

My very own thread pool implementation. It drives pthreads.

This project lives on http://github.com/alzwded/JakWorkers

Use cases
=========

TODO

* continuation
* delayed execution

The checkbox thing means it has one (or more) test case(s).

* [x] Data parallelization (e.g. create an array of 1M floats, compute the squares on 4 workers) -> EXIT_WHEN_ALL_JOBS_COMPLETE = 0
* [x] Performing separate tasks which don't inter-block themselves -> EXIT_WHEN_ALL_JOBS_COMPLETE = 1
* [ ] Continuation (TBD) (also, TODO)
* [ ] Delayed execution (TBD) (also, TODO)

Continuation
============

Yeah... the code you put in a worker should not, in turn, block, or you might bring the entire framework to a grinding halt. You should instead try to block, and if you can't, add a new job that tries to block and continues where you left off.

TODO In the future, I might add a flag on a job that might be blocking which will mean that the framework will try to keep at least one worker which undergoes only jobs which promise to never block. This way, there will be at least one worker that will never block, which means stuff will get done, which may mean that the other blocked threads will be able to continue where they left off assuming no dead-locks.

If you create jobs which inter-lock themselves on mutexes and expect the dispatcher to somehow magically guess what order they need to be run in in order to avoid dead-locks, that's your problem. This framework is not necessarily designed to perform tasks which are SUPER interdependent. Also, if you manage to have an application which requires inter-locking jobs, it means either a) you're doing it wrong, or b) you're doing it right, but what you need is something more complex than JakWorkers.

Caller
======

There are a few ways to use JakWorkers:
1. for a pre-defined amount of tasks
2. as a background process, with exit on demand
3. for a limitted amount of tasks, which is computed at run-time (not tested)

For the first case, you need to do the following:
```C
jw_config_t config = JW_CONFIG_INITIALIZER;
config.EXIT_WHEN_ALL_JOBS_COMPLETE = 1;
jw_init(config);
jw_add_task(&my_task, &data[0]);
jw_add_task(&my_task, &data[1]);
jw_add_task(&my_task, &data[2]);
jw_add_task(&my_task, &data[3]);
jw_main();
printf("All processing complete!\n");
```
The my_task function and data are left as an excercise.

For the background process case, you should do the following:
```C
void main_loop(void* data)
{
    while(!feof(stdin)) {
        char c = getc();
        switch(c) {
        case 'q': jw_exit(0); break;
        case '1': jw_add_task(&task1, NULL); break;
        default: jw_add_task(&task2, &c); break;
        }
    }
    jw_exit(0);
}

main() {
    jw_config_t config = JW_CONFIG_INITIALIZER;
    config.EXIT_WHEN_ALL_JOBS_COMPLETE = 0;
    config.numWorkers = 3;
    jw_init(config);
    jw_add_task(&main_loop, NULL);
    jw_main();
    //...
}
```

For the last case, you can essentially do the same as the first example, but this case implies that a task/job adds a new job to the job queue right before it finishes. Again, this is not tested. If you have tested it, feel free to write me a message.

Callee
======

Really easy to implement. For example:
```C
void my_code(void* data) {
    int* datadata = (int*)data;
    do_stuff(data[0], &data[1]);
}
```

Or you can do something more complicated (and not tested):
```C
void my_code(void* data) {
    int* datadata = (int*)data;
    do_stuff(data[0], &data[1]);
    if(data[0]++ < 10) jw_add_task(&my_code, data);
}
```

Framework
=========

Feel free to read `JakWorker.c`. It's supposed to read like a story. A... choose-your-own-adventure kind of story, with the decisions being time-multiplexed.

I will be updating this story periodically.

Blocking situations
===================

| where | when | action required to esccape |
|-------|------|----------------------------|
| `jw_job_add` | other call to `jw_job_add`, framework's currently dispatching a job | wait |
| `jw_main` | `sem_take(jw_queueSem)` | a worker returns itself to the pool |
| `jw_main` | `lock jw_jobQueue` | calls to add_job end; call to `jw_exit` from another thread; wait |
| `jw_main` | queue is empty && !EXIT_WHEN_ALL_JOBS_COMPLETE | call to add_job |
| `jw_worker` | `pthread_cond_wait` | fw dispatches a job |
| `jw_exit` | `pthread_mutex_lock` ; exit is called from another thread but main thread is NOT waiting for jobAdded condition, but it has the queue locked anyway | main thread unlocks queue if it notices exit was called in order to allow exit to finish, then waits for exit to finish before cleaning up everything |
