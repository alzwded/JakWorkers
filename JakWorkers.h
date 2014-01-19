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

#ifndef JAK_WORKERS_H
#define JAK_WORKERS_H

#define JW_CONFIG_INITIALIZER { 4, 1 }

typedef struct {
    int numWorkers;
    unsigned char EXIT_WHEN_ALL_JOBS_COMPLETE;
} jw_config_t;

extern int jw_init(jw_config_t const config);

// thread pool main loop. 
// the return value is set with jw_exit
extern int jw_main();

typedef void (*jw_job_func_t)(void*);

// add a new job to the pool
// The job should not arbitrarily block unless you want a beautiful
// deadlock...
// Returns a non-zero value if the job could not be added because the
// framework is currently shutting down (or it has already shut down)
extern int jw_add_job(jw_job_func_t, void* data);

// stop the pool thing and clean up resources
// you should not call this unless you know what you're doing
extern int jw_exit(int);

#endif
