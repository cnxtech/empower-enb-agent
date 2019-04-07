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

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "agent.h"

#define TASK_NET_ERROR   -1 /* Error at netwrok level during task processing */
#define TASK_CONSUMED     0 /* Task consumed */
#define TASK_NOT_ELAPSED  1 /* Too early for this task */
#define TASK_RESCHEDULE   2 /* Task to be rescheduled */

/* Dif "b-a" two timespec structs and return such value in ms*/
#define ts_diff_to_ms(a, b)                     \
        (((b->tv_sec - a->tv_sec) * 1000) +     \
        ((b->tv_nsec - a->tv_nsec) / 1000000))

/******************************************************************************
 * Locking and atomic context                                                 *
 ******************************************************************************/

/* Procedure:
 *      sched_lock_ctx
 *
 * Abstract:
 *      Lock a scheduling context
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx - The context to lock
 *
 * Returns:
 *      ---
 */
#define sched_lock_ctx(ctx)       pthread_spin_lock(&ctx->lock)

/* Procedure:
 *      sched_unlock_ctx
 *
 * Abstract:
 *      Unlock a scheduling context
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx - The context to unlock
 *
 * Returns:
 *      ---
 */
#define sched_unlock_ctx(ctx)     pthread_spin_unlock(&ctx->lock)

/******************************************************************************
 * Utilities                                                                  *
 ******************************************************************************/

/* Procedure:
 *      sched_to_RANconf
 *
 * Abstract:
 *      Translates protocol information into RAN configuration one that will be
 *       used to emit feedback through the callback system.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      det  - Details incoming from the protocol
 *      conf - Configuration used with the callback system
 *
 * Returns:
 *      ---
 */

INTERNAL
void
sched_to_RANconf(ep_ran_slice_det * det, em_RAN_conf * conf)
{
        int i;

        conf->l2.user_sched = det->l2.usched;
        conf->l2.rbg        = det->l2.rbgs;

        for(i = 0; i < det->nof_users && i < EP_RAN_USERS_MAX; i++) {
                conf->users[i] = det->users[i];
        }

        conf->nof_users = det->nof_users;
}

/* Procedure:
 *      sched_send_msg
 *
 * Abstract:
 *      Send a message to the controller connected with the given agent.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      agent - Agent context to consider
 *
 * Returns:
 *      A TASK_* error code to identify what to do with the task
 */
INTERNAL
int
sched_send_msg(emage * agent, char * msg, unsigned int size)
{
        if(size > EM_BUF_SIZE) {
                EMLOG(agent, "Message too long, msg=%lu, limit=%d!\n",
                        size + sizeof(uint32_t), EM_BUF_SIZE);

                return TASK_CONSUMED;
        }

        /* Insert the correct sequence number before sending */
        epf_seq(msg, size, net_next_seq(&agent->net));

        EMDBG(agent, "Sending a message of %d bytes...\n", size);

        if(net_send(&agent->net, msg, size) < 0) {
                return TASK_NET_ERROR; /* On error */
        } else {
                return TASK_CONSUMED;  /* On success */
        }
}

/******************************************************************************
 * Tasks                                                                      *
 ******************************************************************************/

/* Procedure:
 *      sched_perform_send
 *
 * Abstract:
 *      Perform a sending task, which will send a message to the controller.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      agent - The agent to operate on
 *      task  - Task to consume
 *
 * Returns:
 *      A TASK_* error code to identify what to do with the task.
 */
INTERNAL
int
sched_perform_send(emage * agent, emtask * task)
{
        return sched_send_msg(agent, task->msg, task->msg_size);
}

/* Procedure:
 *      sched_perform_enb_setup
 *
 * Abstract:
 *      Perform an eNB Setup task.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      agent - The agent to operate on
 *      task  - Task to consume
 *
 * Returns:
 *      A TASK_* error code to identify what to do with the task.
 */
INTERNAL
int
sched_perform_enb_setup(emage * agent, emtask * task)
{
        uint32_t mod = 0;

        if(epp_head(task->msg, task->msg_size, 0, 0, 0, &mod, 0)) {
                return TASK_CONSUMED;
        }

        EMDBG(agent, "Performing eNB Capability\n");

        if(agent->ops && agent->ops->enb_setup_request) {
                agent->ops->enb_setup_request(mod);
        }

        return TASK_CONSUMED;
}

/* Procedure:
 *      sched_perform_ho
 *
 * Abstract:
 *      Perform an Handover task.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      agent - The agent to operate on
 *      task  - Task to consume
 *
 * Returns:
 *      A TASK_* error code to identify what to do with the task.
 */
INTERNAL
int
sched_perform_ho(emage * agent, emtask * task)
{
        uint32_t mod   = 0;
        uint16_t scell = 0;
        uint16_t rnti  = 0;
        uint64_t tenb  = 0;
        uint16_t tcell = 0;
        uint8_t  cause = 0;

        EMDBG(agent, "Performing Handover\n");

        if(epp_head(task->msg, task->msg_size, 0, 0, &scell, &mod, 0)) {
                return TASK_CONSUMED;
        }

        if(epp_single_ho_req(
                task->msg,
                task->msg_size,
                &rnti,
                &tenb,
                &tcell,
                &cause))
        {
                return TASK_CONSUMED;
        }

        if(agent->ops && agent->ops->handover_UE) {
                agent->ops->handover_UE(mod, scell, rnti, tenb, tcell, cause);
        }

        return TASK_CONSUMED;
}

/* Procedure:
 *      sched_perform_ue_measure
 *
 * Abstract:
 *      Perform an UE Measurement task.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      agent - The agent to operate on
 *      task  - Task to consume
 *
 * Returns:
 *      A TASK_* error code to identify what to do with the task.
 */
INTERNAL
int
sched_perform_ue_measure(emage * agent, emtask * task)
{
        emtri *    trig;
        ep_ue_meas m;

        EMDBG(agent, "Performing UE measurements\n");

        if(agent->ops && agent->ops->ue_measure) {
                trig = trig_find_by_id(&agent->trig, task->trig);

                if(!trig) {
                        EMLOG(agent, "Trigger %d not found\n", task->trig);
                        return TASK_CONSUMED;
                }

                if(epp_trigger_uemeas_req(trig->msg, trig->msg_size, &m)) {
                        return TASK_CONSUMED;
                }

                /* Do it for every measure received */
                for(; m.nof_rrc > 0; m.nof_rrc--) {
                        agent->ops->ue_measure(
                                trig->mod,
                                trig->id,
                                m.rrc[m.nof_rrc - 1].meas_id,
                                m.rrc[m.nof_rrc - 1].rnti,
                                m.rrc[m.nof_rrc - 1].earfcn,
                                m.rrc[m.nof_rrc - 1].interval,
                                m.rrc[m.nof_rrc - 1].max_cells,
                                m.rrc[m.nof_rrc - 1].max_meas);
                }
        }

        return TASK_CONSUMED;
}

/* Procedure:
 *      sched_perform_cell_meas
 *
 * Abstract:
 *      Perform a cell measurement task.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      agent - The agent to operate on
 *      task  - Task to consume
 *
 * Returns:
 *      A TASK_* error code to identify what to do with the task.
 */
INTERNAL
int
sched_perform_cell_meas(emage * agent, emtask * task)
{
        mod_id_t  mod  = 0;
        int32_t   intv;
        cell_id_t cell;
        emtri *   trig = 0;

        EMDBG(agent, "Performing a cell measurement job\n");

        if(agent->ops && agent->ops->cell_measure) {
                /* Trigger-type cell measurement */
                if(task->trig > 0) {
                        trig = trig_find_by_id(&agent->trig, task->trig);

                        if(!trig) {
                                return TASK_CONSUMED;
                        }

                        if(epp_head(
                                trig->msg,
                                trig->msg_size,
                                0,
                                0,
                                &cell,
                                0,
                                0))
                        {
                                return TASK_CONSUMED;
                        }

                        if(epp_trigger_cell_meas_req(
                                trig->msg, trig->msg_size))
                        {
                                return TASK_CONSUMED;
                        }

                        /* Trigger type does not report intervals */
                        agent->ops->cell_measure(cell, trig->mod, 0, trig->id);
                } else {
                        if(epp_head(
                                task->msg,
                                task->msg_size,
                                0,
                                0,
                                &cell,
                                &mod,
                                0))
                        {
                                return TASK_CONSUMED;
                        }

                        if(epp_sched_cell_meas_req(
                                task->msg, task->msg_size, &intv))
                        {
                                return TASK_CONSUMED;
                        }

                        /* Trigger type does not report intervals */
                        agent->ops->cell_measure(cell, mod, intv, -1);
                }
        }

        return TASK_CONSUMED;
}

/* Procedure:
 *      sched_perform_ue_report
 *
 * Abstract:
 *      Perform an UE Report task.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      agent - The agent to operate on
 *      task  - Task to consume
 *
 * Returns:
 *      A TASK_* error code to identify what to do with the task.
 */
INTERNAL
int
sched_perform_ue_report(emage * agent, emtask * task)
{
        uint32_t mod = 0;

        EMDBG(agent, "Performing UE Report\n");

        if(agent->ops && agent->ops->ue_report) {
                agent->ops->ue_report(task->mod, (int)task->trig);
        }

        return TASK_CONSUMED;
}

/* Procedure:
 *      sched_perform_hello
 *
 * Abstract:
 *      Perform an Hello task.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      agent - The agent to operate on
 *      task  - Task to consume
 *
 * Returns:
 *      A TASK_* error code to identify what to do with the task.
 */
INTERNAL
int
sched_perform_hello(emage * agent, emtask * task)
{
        char buf[64] = {0};
        int  blen = 0;

        EMDBG(agent, "Performing Hello\n");

        blen = epf_sched_hello_req(
                buf, 64, agent->enb_id, 0, 0, task->elapse, 0);

        if(blen < 0) {
                EMLOG(agent, "Cannot parse Hello, error %d", blen);
                return TASK_CONSUMED;
        }

        return sched_send_msg(agent, buf, (unsigned int)blen);
}

/* Procedure:
 *      sched_perform_ran_setup
 *
 * Abstract:
 *      Perform a RAN Setup task.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      agent - The agent to operate on
 *      task  - Task to consume
 *
 * Returns:
 *      A TASK_* error code to identify what to do with the task.
 */
INTERNAL
int
sched_perform_ran_setup(emage * agent, emtask * task)
{
        uint32_t mod = 0;

        EMDBG(agent, "Performing RAN Setup\n");

        epp_head(task->msg, task->msg_size, 0, 0, 0, &mod, 0);

        if (agent->ops && agent->ops->ran.setup_request) {
                agent->ops->ran.setup_request(mod);
        }

        return TASK_CONSUMED;
}

/* Procedure:
 *      sched_perform_ran_slice
 *
 * Abstract:
 *      Perform a RAN Slice task.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      agent - The agent to operate on
 *      task  - Task to consume
 *
 * Returns:
 *      A TASK_* error code to identify what to do with the task.
 */
INTERNAL
int
sched_perform_ran_slice(emage * agent, emtask * task)
{
        ep_msg_type       type = 0;
        ep_act_type       act  = 0;
        ep_op_type        op   = 0;
        uint32_t          mod  = 0;

        uint64_t         id    = 0;
        ep_ran_slice_det sdet  = {0};
        em_RAN_conf      conf  = {0};

        /* If no operations are there, dont perform any other job. */
        if (!agent->ops) {
                return TASK_CONSUMED;
        }

        epp_head(task->msg, task->msg_size, &type, 0, 0, &mod, 0);
        act = epp_single_type(task->msg, task->msg_size);
        op  = epp_single_op(task->msg, task->msg_size);

        /* Depending on the operation requested, call the correct callback */
        switch (op) {
        /* A request */
        case EP_OPERATION_UNSPECIFIED:
                EMDBG(agent, "Performing RAN Slice request\n");

                if (agent->ops->ran.slice_request) {
                        if(epp_single_ran_slice_req(
                                task->msg, task->msg_size, &id))
                        {
                                break;
                        }

                        agent->ops->ran.slice_request(mod, id);
                }
                break;
        /* An addition */
        case EP_OPERATION_ADD:
                EMDBG(agent, "Performing RAN Slice addition\n");

                if (agent->ops->ran.slice_add) {
                        if(epp_single_ran_slice_add(
                                task->msg,
                                task->msg_size,
                                &id,
                                &sdet))
                        {
                                break;
                        }

                        sched_to_RANconf(&sdet, &conf);
                        agent->ops->ran.slice_add(mod, id, &conf);
                }

                break;
        /* A remove */
        case EP_OPERATION_REM:
                EMDBG(agent, "Performing RAN Slice removal\n");

                if (agent->ops->ran.slice_rem) {
                        if(epp_single_ran_slice_rem(
                                task->msg, task->msg_size, &id))
                        {
                                break;
                        }

                        agent->ops->ran.slice_rem(mod, id);
                }

                break;
        /* Parameter settings */
        case EP_OPERATION_SET:
                EMDBG(agent, "Performing RAN Slice set\n");

                if (agent->ops->ran.slice_conf) {
                        if(epp_single_ran_slice_set(
                                task->msg, task->msg_size, &id, &sdet))
                        {
                                break;
                        }

                        sched_to_RANconf(&sdet, &conf);
                        agent->ops->ran.slice_conf(mod, id, &conf);
                }

                break;
        }

        return TASK_CONSUMED;
}

/******************************************************************************
 * Generic procedures:                                                        *
 ******************************************************************************/

/* Procedure:
 *      sched_add_task
 *
 * Abstract:
 *      Adds a task in a safe way to a context. The task will be granted to
 *      sleep at least for its specified interval before being executed. The
 *      wait is not perfect since it follows scheduler clock quantums.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx  - Context to operate on
 *      task - Task to add
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
sched_add_task(schctx * ctx, emtask * task)
{
#ifdef EBUG
        emage * a = container_of(ctx, emage, sched);
#endif

        clock_gettime(CLOCK_REALTIME, &task->issued);

/****** Start of the critical section *****************************************/
        sched_lock_ctx(ctx);

        /* Perform the job if the context is not stopped. */
        if(!ctx->stop) {
                list_add_tail(&task->next, &ctx->jobs);
        } else {
                sched_unlock_ctx(ctx); /* Unlocking *************************/
                return -1;
        }

        sched_unlock_ctx(ctx);
/****** End of the critical section *******************************************/

        EMDBG(a, "Task %d added to scheduler\n", task->type);

        return 0;
}

/* Procedure:
 *      sched_perform_job
 *
 * Abstract:
 *      Perform a single task, processing the necessary operation to consume it.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      agent - Context to operate on
 *      task  - Task to add
 *      now   - Time at which the job is performed
 *
 * Returns:
 *      A TASK_* error code to identify what to do with the task.
 */
INTERNAL
int
sched_perform_job(emage * agent, emtask * task, struct timespec * now)
{
        int               s  = TASK_CONSUMED;
        struct timespec * is = &task->issued;

        /* Task not to be performed right now */
        if(ts_diff_to_ms(is, now) < task->elapse) {
                return TASK_NOT_ELAPSED;
        }

        switch(task->type) {
        case TASK_TYPE_SEND:
                s = sched_perform_send(agent, task);
                break;
        case TASK_TYPE_HELLO:
                s = sched_perform_hello(agent, task);
                break;
        case TASK_TYPE_ENB_SETUP:
                s = sched_perform_enb_setup(agent, task);
                break;
        case TASK_TYPE_UE_REPORT:
                s = sched_perform_ue_report(agent, task);
                break;
        case TASK_TYPE_UE_MEASURE:
                s = sched_perform_ue_measure(agent, task);
                break;
        case TASK_TYPE_CELL_MEASURE:
                s = sched_perform_cell_meas(agent, task);
                break;
        case TASK_TYPE_HO:
                s = sched_perform_ho(agent, task);
                break;
        case TASK_TYPE_RAN_SETUP:
                s = sched_perform_ran_setup(agent, task);
                break;
        case TASK_TYPE_RAN_SLICE:
                s = sched_perform_ran_slice(agent, task);
                break;
        default:
                EMDBG(agent, "Unknown task of type %d\n", task->type);
        }

        /* The job has to be rescheduled? */
        if(s == TASK_CONSUMED && task->resch) {
                EMDBG(agent, "The task will be rescheduled!\n");
                return TASK_RESCHEDULE;
        }

        return s;
}

/* Procedure:
 *      sched_consume
 *
 * Abstract:
 *      Tries to consume the tasks assigned for this quantum. The procedure will
 *      discard consumed task and reschedule task which have not elapsed yet or
 *      are requested to occurs again.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx - Context to operate on
 *
 * Returns:
 *      A TASK_* error code to identify what to do with the task
 */
INTERNAL
int
sched_consume(schctx * ctx)
{
        emage *         a   = container_of(ctx, emage, sched);
        netctx *        net = &a->net;
        emtask *        task= 0;
        emtask *        tmp = 0;
        struct timespec now;
        int             op = 0;
        int             nt = 1;  /* New task to consume. */
        int             ne = 0;  /* Network error. */

        while(nt) {
/****** Start of the critical section *****************************************/
                sched_lock_ctx(ctx);

                /* Nothing to to? Go to sleep. */
                if(list_empty(&ctx->jobs)) {
                        nt = 0;
                }

                if(nt) {
                        task = list_first_entry(&ctx->jobs, emtask, next);
                        list_del(&task->next);
                }

                sched_unlock_ctx(ctx);
/****** End of the critical section *******************************************/

                /* Nothing to do... out! */
                if(!nt) {
                        break;
                }

                clock_gettime(CLOCK_REALTIME, &now);

                op = sched_perform_job(a, task, &now);

/****** Start of the critical section *****************************************/
                sched_lock_ctx(ctx);

                /* Possible outcomes. */
                switch(op) {
                case TASK_NOT_ELAPSED:
                        list_add(&task->next, &ctx->todo);
                        break;
                case TASK_RESCHEDULE:
                        task->issued.tv_sec  = now.tv_sec;
                        task->issued.tv_nsec = now.tv_nsec;
                        list_add(&task->next, &ctx->todo);

                        /* Consume one reschedule credit */
                        if(task->resch > 0) {
                                task->resch--;
                        }

                        break;
                case TASK_CONSUMED:
                        task_free(task);
                        break;
                case TASK_NET_ERROR:
                        task_free(task);
                        ne = 1;
                        break;
                }

                /* Network error happened? */
                if(ne) {
                        /* Dump jobs to process at next run */
                        list_for_each_entry_safe(task, tmp, &ctx->todo, next) {
                                list_del(&task->next);
                                task_free(task);
                        }

                        /* Free ANY remaining job still to process */
                        list_for_each_entry_safe(task, tmp, &ctx->jobs, next) {
                                list_del(&task->next);
                                task_free(task);
                        }

                        sched_unlock_ctx(ctx); /* Unlocking *******************/

                        trig_flush(&a->trig);

                        /* Alert wrapper about controller disconnection */
                        if(a->ops->disconnected) {
                                a->ops->disconnected();
                        }

                        /* Signal the network that the connection is now down */
                        net_not_connected(net);

                        return 0;
                }

                sched_unlock_ctx(ctx);
/****** End of the critical section *******************************************/
        }

        /* All the tasks marked as to process again are moved to the official
         * job queue.
         */

/****** Start of the critical section *****************************************/
        sched_lock_ctx(ctx);

        list_for_each_entry_safe(task, tmp, &ctx->todo, next) {
                list_del(&task->next);
                list_add(&task->next, &ctx->jobs);
        }

        sched_unlock_ctx(ctx);
/****** End of the critical section *******************************************/

        return 0;
}

/******************************************************************************
 * Scheduler procedures.                                                      *
 ******************************************************************************/

/* Procedure:
 *      sched_loop
 *
 * Abstract:
 *      Loop through the scheduling context and consume/reschedule tasks.
 *      All the task added to the scheduling context will be processed within
 *      this thread, so callback wrapper impementation will run at this level.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      args - Scheduling context to operate on
 *
 * Returns:
 *      ---
 */
INTERNAL
void *
sched_loop(void * args)
{
        schctx *        s   = (schctx *)args;
#ifdef EBUG
        emage *        a   = container_of(s, emage, sched);
#endif
        unsigned int    wi  = s->interval;
        struct timespec wt  = {0};
        struct timespec td  = {0};
        emtask *        task= 0;
        emtask *        tmp = 0;

        /* Convert the wait interval in a timespec struct. */
        while(wi >= 1000) {
                wi -= 1000;
                wt.tv_sec += 1;
        }
        wt.tv_nsec = wi * 1000000;

        EMDBG(a, "Scheduling loop starting, interval=%d ms\n", s->interval);

        while(!s->stop) {
                /* Job scheduling logic. */
                sched_consume(s);
                /* Relax the CPU. */
                nanosleep(&wt, &td);
        }

        EMDBG(a, "Scheduling loop stopping\n");

/****** Start of the critical section *****************************************/
        sched_lock_ctx(s);

        /* Dump task to process again. */
        list_for_each_entry_safe(task, tmp, &s->todo, next) {
                list_del(&task->next);
                task_free(task);
        }

        /* Free ANY remaining job still to process. */
        list_for_each_entry_safe(task, tmp, &s->jobs, next) {
                list_del(&task->next);
                task_free(task);
        }

        sched_unlock_ctx(s);
/****** End of the critical section *******************************************/

        /*
         * If execution arrives here, then a stop has been issued.
         */

        return 0;
}

/* Procedure:
 *      sched_start
 *
 * Abstract:
 *      Initializes and starts a scheduling context.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx - Scheduling context to operate on
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
sched_start(schctx * ctx)
{
        emage * a = container_of(ctx, emage, sched);

        ctx->interval = 100;

        INIT_LIST_HEAD(&ctx->jobs);
        INIT_LIST_HEAD(&ctx->todo);

        pthread_spin_init(&ctx->lock, 0);

        /* Create the context where the agent scheduler will run on */
        if(pthread_create(&ctx->thread, NULL, sched_loop, ctx)) {
                EMLOG(a, "Cannot create scheduler thread\n");
                return -1;
        }

        return 0;
}

/* Procedure:
 *      sched_stop
 *
 * Abstract:
 *      Stop a previously started thread. The call will wait for the thread to
 *      finish its operations before returning to the caller.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx - Scheduling context to operate on
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
sched_stop(schctx * ctx)
{
#ifdef EBUG
        emage * a = container_of(ctx, emage, sched);
#endif

        EMDBG(a, "Stopping scheduling context\n");

        /* Stop and wait for it... */
        ctx->stop = 1;

        pthread_join(ctx->thread, 0);
        pthread_spin_destroy(&ctx->lock);

        return 0;
}
