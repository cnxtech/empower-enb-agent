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

#ifndef __EMAGE_TASK_H
#define __EMAGE_TASK_H

#include <time.h>

#include "agent.h"

/* Possible types of jobs to issue in the scheduler */
enum TASK_TYPES {
        TASK_TYPE_INVALID = 0,
        TASK_TYPE_SEND,
        TASK_TYPE_HELLO,
        TASK_TYPE_ENB_SETUP,
        TASK_TYPE_UE_REPORT,
        TASK_TYPE_UE_MEASURE,
        TASK_TYPE_CELL_MEASURE,
        TASK_TYPE_HO,
        TASK_TYPE_RAN_SETUP,
        TASK_TYPE_RAN_SLICE,
};

/* Structure:
 *      emjob
 *
 * Abstract:
 *      Provides the description of how a job is organized in emage.
 *
 * Critical sections:
 *      ---
 */
typedef struct __emage_task {
        /* Member of a list */
        struct list_head next;

        /* Id of this job */
        task_id_t        id;
        /* Type of task scheduled */
        int              type;
        /* Module associated with this task */
        mod_id_t         mod;

        /* Original message associated with the task (if any) */
        char *           msg;
        /* Size of the associated message */
        unsigned int     msg_size;

        /* Trigger associated with the task */
        trig_id_t        trig;

        /* This variable contains the number of time a message will be
         * rescheduled; -1 cause the job to be re-scheduled forever.
         */
        int              resch;

        /* Time when the job has been enqueued */
        struct timespec  issued;
        /* Time in 'ms' after that the job will be run */
        int              elapse;
} emtask;

/* Creates and returns a newly created task */
INTERNAL emtask * task_alloc(
        int           type,
        int           interval,
        int           res,
        trig_id_t     trigger,
        char *        msg,
        unsigned int  size);

/* Free a task and all the associated resources */
INTERNAL void     task_free(emtask * task);

#endif /* __EMAGE_TASK_H */
