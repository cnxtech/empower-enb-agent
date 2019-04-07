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

#include <stdlib.h>
#include <string.h> /* For memset/memcpy mainly */

#include "agent.h"

/* Very minimal memory reporting mechanism */
#ifdef EBUG_MEMORY

/* Total amount of memory used in task creation */
unsigned int task_mem    = 0;
/* Memory has overflown? (unlikely, but still register it) */
int          task_mem_OF = 0;

#endif /* EBUG_MEMORY */

/* Procedure:
 *      task_alloc
 *
 * Abstract:
 *      Allocate a new task that will be performed later by a schedueler. This
 *      procedure mainly takes into account resource allocation and task format.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      type     - The type of task
 *      interval - Time in ms after that the task will be performed
 *      res      - How many time has to be rescheduled? -1 for infinite
 *      trigger  - ID of the trigger associate with the task
 *      msg      - Original message
 *      size     - Size of the original message
 *
 * Returns:
 *      Pointer to the created task on success, otherwise 0 on errors.
 */
INTERNAL
emtask *
task_alloc(
        int           type,
        int           interval,
        int           res,
        trig_id_t     trigger,
        char *        msg,
        unsigned int  size)
{
        emtask * ret = malloc(sizeof(emtask));

        if(!ret) {
                return 0;
        }

        memset(ret, 0, sizeof(emtask));

        INIT_LIST_HEAD(&ret->next);
        ret->type   = type;
        ret->elapse = interval;
        ret->resch  = res;

        /* Any message to save within the task? */
        if(msg) {
                ret->msg = malloc(size);

                if(!ret->msg) {

                        free(ret);
                        return 0;
                }

                memcpy(ret->msg, msg, size);
                ret->msg_size = size;
        }

        /* Any trigger to associate to the task? */
        if(trigger) {
                ret->trig = trigger;
        }

#ifdef EBUG_MEMORY
        /* Memory overflow, which means more than 4GB just for this! */
        if(task_mem + sizeof(emtask) + size < task_mem) {
                task_mem_OF++;
        }

        task_mem += sizeof(emtask) + size;

        printf("Tasks used memory is %09d bytes; overflow? %d\n",
                task_mem,
                task_mem_OF);
#endif  /* EBUG_MEMORY */

        return ret;
}

/* Procedure:
 *      task_free
 *
 * Abstract:
 *      Release the resources associated with a task.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      task - Pointer to the task to release
 *
 * Returns:
 *      ---
 */
INTERNAL
void
task_free(emtask * task)
{
        if(!task) {
                return;
        }

#ifdef EBUG_MEMORY
        task_mem -= sizeof(emtask) + task->msg_size;

        printf("Tasks used memory is %09d bytes; overflow? %d\n",
                task_mem,
                task_mem_OF);
#endif  /* EBUG_MEMORY */

        if(task->msg) {
                free(task->msg);
        }

        free(task);

        return;
}
