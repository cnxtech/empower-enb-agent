/* Copyright (c) 2019 FBK
 * Designed by Roberto Riggio
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __EMAGE_SCHEDULER_H
#define __EMAGE_SCHEDULER_H

/* See task.h for more details */
typedef struct __emage_task emtask;

/* Structure:
 *      schctx
 *
 * Abstract:
 *      Provides an area of memory where scheduler context state machine is
 *      saved.
 *
 * Critical sections:
 *      Initialization of this element should be performed under a critical area
 *      to ensure proper initialization or release. Once started the structure
 *      provides the necessary element for the scheduling context to perform
 *      in a save way its jobs.
 */
typedef struct __emage_scheduler_context {
        /* A value different than 0 stop this context */
        int                stop;

        /* Tasks actually active in the scheduler */
        struct list_head   jobs;
        /* Tasks to do but not scheduled for this run */
        struct list_head   todo;

        /* Thread in charge of this listening */
        pthread_t          thread;
        /* Lock for elements of this context */
        pthread_spinlock_t lock;
        /* Time to wait at the end of each loop, in ms */
        unsigned int       interval;
} schctx;

/* Adds a job to a scheduler context */
INTERNAL int      sched_add_task(schctx * ctx, emtask * task);

/* Find and return the job instance with matching id and type */
//INTERNAL emtask * sched_find_job(schctx * ctx, task_id_t id, int type);

/* Release a job which is currently scheduled by using the associated id */
//INTERNAL int      sched_remove_job(schctx * ctx, task_id_t id, int type);

/* Correctly start a new scheduler in it's own context */
INTERNAL int      sched_start(schctx * ctx);

/* Stop a scheduler and release all its resources */
INTERNAL int      sched_stop(schctx * ctx);

#endif /* __EMAGE_SCHEDULER_H */
