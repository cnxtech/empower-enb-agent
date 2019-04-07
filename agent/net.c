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

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <emage.h>

#include "agent.h"

#define NET_WAIT_TIME 100 /* In ms */

/******************************************************************************
 * Network procedures.                                                        *
 ******************************************************************************/

/* Procedure:
 *      net_connected
 *
 * Abstract:
 *      Perform a common set of operations once a TCP connection with an
 *      endpoint is opened correctly. At this state we still don't know if the
 *      target endpoint is a controller; we just opened a TCP socket towards it.
 *
 * Assumptions:
 *      Arguments are valid.
 *
 * Arguments:
 *      ctx - The network context to operate on
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
net_connected(netctx * ctx)
{
        emage *  a = container_of(ctx, emage, net);
        emtask * h = task_alloc(TASK_TYPE_HELLO, 2000, -1, 0, 0, 0);

        if(!h) {
                return -1;
        }

        /* Schedule the Hello task, which sends hello messages at a well defined
         * time interval.
         */
        if(sched_add_task(&a->sched, h)) {
                task_free(h);
                return -1;
        }

        ctx->status = NET_STATUS_CONNECTED;

        EMDBG(a, "Connected to endpoint %s:%d\n", ctx->addr, ctx->port);

        return 0;
}

/* Procedure:
 *      net_next_seq
 *
 * Abstract:
 *      Extract the sequence number to use next in networking operations, while
 *      keeping it synchronized.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx - Network context to operate on
 *
 * Returns:
 *      A valid sequence number.
 */
INTERNAL
seq_id_t
net_next_seq(netctx * ctx)
{
        int ret = 0;

        if(!ctx) {
                return 0;
        }

        pthread_spin_lock(&ctx->lock);
        ret = ctx->seq++;
        pthread_spin_unlock(&ctx->lock);

        return ret;
}

/* Procedure:
 *      net_noblock_socket
 *
 * Abstract:
 *      Set a socket descriptor to non-blocking mode.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      sockfd - The socket descriptor to operate on.
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
net_noblock_socket(int sockfd)
{
        int flags = fcntl(sockfd, F_GETFL, 0);

        if(flags < 0) {
                return -1;
        }
        return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

/* Procedure:
 *      net_nodelay_socket
 *
 * Abstract:
 *      Set a socket descriptor to non-delay mode. This allow TCP connection to
 *      not use buffer and immediately send given data.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      sockfd - The socket descriptor to operate on.
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */

INTERNAL
int
net_nodelay_socket(int sockfd)
{
        int flag = 1; /* Enable no delay... */

        int result = setsockopt(
                sockfd,
                SOL_TCP,
                TCP_NODELAY,
                (char *) &flag,
                sizeof(int));

        if (result < 0) {
                return result;
        }

        return 0;
}

/* Procedure:
 *      net_not_connected
 *
 * Abstract:
 *      Setup the network layer to accomodate a disconnection event.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx - The network context to operate on.
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */

INTERNAL
int
net_not_connected(netctx * ctx)
{
#ifdef EBUG
        emage * a = container_of(ctx, emage, net);
#endif
        if(ctx->sockfd > 0) {
                close(ctx->sockfd);
                ctx->sockfd = -1;
        }

        ctx->status = NET_STATUS_NOT_CONNECTED;
        ctx->seq    = 0;

        EMDBG(a, "Network no more connected!\n");

        return 0;
}

/* Procedure:
 *      net_connect_to_controller
 *
 * Abstract:
 *      Performs standard operations to connect with the controller. Setup the
 *      state machine of the given context in the correct way on both success or
 *      failure.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx - The network context to operate on.
 *
 * Returns:
 *      0 on success, otherwise a negative error calue.
 */
INTERNAL
int
net_connect_to_controller(netctx * ctx)
{
        emage *            agent   =  container_of(ctx, emage, net);
        int                flags   =  0;
        int                status  =  0;
        struct sockaddr_in srvaddr = {0};
        struct hostent *   ctrli   =  0;

        if(ctx->sockfd < 0) {
                ctx->sockfd = socket(AF_INET, SOCK_STREAM, 0);

                if(ctx->sockfd < 0) {
                        EMLOG(agent, "Cannot create socket\n");
                        return -1;
                }

                net_noblock_socket(ctx->sockfd);
                net_nodelay_socket(ctx->sockfd);
        }

        ctrli = gethostbyname(ctx->addr);

        if(!ctrli) {
                EMLOG(agent, "Cannot resolve controller %.16s\n", ctx->addr);
                return -1;
        }

        srvaddr.sin_family = AF_INET;
        memcpy(&srvaddr.sin_addr.s_addr, ctrli->h_addr, ctrli->h_length);
        srvaddr.sin_port   = htons(ctx->port);

again:
        if(ctx->stop) {
                EMLOG(agent, "Network is stopping!\n");
                return -1;
        }

        status = connect(
                ctx->sockfd,
                (struct sockaddr *)&srvaddr,
                sizeof(struct sockaddr));

        if(status < 0) {
                /* Since it's a non-blocking call, in case the connection is
                 * in progress we just try again.
                 */
                if(errno == EALREADY) {
                        usleep(100000); /* 0.1 second */
                        goto again;
                }

                EMLOG(agent, "Cannot connect to %.16s, error=%d\n",
                        ctx->addr, status);

                return -1;
        }

        return 0;
}

/* Procedure:
 *      net_recv
 *
 * Abstract:
 *      Receive data through a network context. Since 'recv' operation can
 *      generate exceptions if problem with the socket arise, this behavior is
 *      disabled here. In addition operation are done in a non-blocking way.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx  - The network context to operate on
 *      buf  - Buffer where to put data
 *      size - Size of the buffer
 *
 * Returns:
 *      Returns 0 if the socket has been closed, a negative number on error or
 *      otherwise the number of received bytes.
 */
INTERNAL
int
net_recv(netctx * ctx, char * buf, unsigned int size)
{
        return recv(ctx->sockfd, buf, size, MSG_DONTWAIT | MSG_NOSIGNAL);
}

/* Procedure:
 *      net_send
 *
 * Abstract:
 *      Send data through a network context. Since 'recv' operation can generate
 *      exceptions if problem with the socket arise, this behavior is disabled
 *      here. In addition operation are done in a non-blocking way.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx  - The network context to operate on
 *      buf  - Buffer to send
 *      size - Size of the buffer to send
 *
 * Returns:
 *      Returns 0 if the socket has been closed, a negative number on error or
 *      otherwise the number of received bytes.
 */
INTERNAL
int
net_send(netctx * ctx, char * buf, unsigned int size)
{
        /* NOTE:
         * Since sending on a dead socket can cause a signal to be issued to the
         * application (SIGPIPE), we don't want that the host get disturbed by
         * this, and so we ask not to notify the error.
         */
        return send(ctx->sockfd, buf, size, MSG_DONTWAIT | MSG_NOSIGNAL);
}

/* Procedure:
 *      net_sched_job
 *
 * Abstract:
 *      Schedule a task inside the schedule context for the given agent.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      agent - Agent where the procedure operate on
 *      task  - Task to include
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
net_sched_task(emage * agent, emtask * task)
{
        EMDBG(agent, "Scheduling a task; type=%d\n", task->type);
        return sched_add_task(&agent->sched, task);
}

/******************************************************************************
 * Message specific procedures.                                               *
 ******************************************************************************/

/* Procedure:
 *      net_sc_hello
 *
 * Abstract:
 *      Perform operations on receiving a scheduled Hello reply.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      net  - The network context to operate on
 *      buf  - Buffer to send
 *      size - Size of the buffer to send
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
net_sc_hello(netctx * ctx, char * msg, int size)
{
        emage *  a = container_of(ctx, emage, net);

        EMDBG(a, "Processing Scheduled Hello\n");
#if 0
        /* Find the Hello job and change its interval.
         *
         * WARNING:
         * We are handling a reference here, and there could be problem is the
         * job is removed from the list. Luckily here Hello is always maintained
         * into the queue, but this can be false for other type of messages.
         */
        j = em_sched_find_job(&a->sched, 0, JOB_TYPE_HELLO);

        if(j) {
                j->elapse = epp_sched_interval(msg, size);
        }
#endif
        return 0;
}

/* Procedure:
 *      net_se_enb_setup
 *
 * Abstract:
 *      Perform operations on receiving a Single eNB Setup request.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx  - The network context to operate on
 *      buf  - Buffer to send
 *      size - Size of the buffer to send
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
net_se_enb_setup(netctx * ctx, char * msg, int size)
{
        emage *  a = container_of(ctx, emage, net);
        emtask * t = task_alloc(TASK_TYPE_ENB_SETUP, 1, 0, 0, msg, size);

        if(!t) {
                return -1;
        }

        EMDBG(a, "Processing eNB Capabilities\n");

        return net_sched_task(a, t);
}

/* Procedure:
 *      net_se_ho
 *
 * Abstract:
 *      Perform operations on receiving a Single Handover request.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx  - The network context to operate on
 *      buf  - Buffer to send
 *      size - Size of the buffer to send
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
net_se_ho(netctx * ctx, char * msg, int size)
{
        emage *  a = container_of(ctx, emage, net);
        emtask * t = task_alloc(TASK_TYPE_HO, 1, 0, 0, msg, size);

        if(!t) {
                return -1;
        }

        EMDBG(a, "Processing Handover\n");

        return net_sched_task(a, t);
}

/* Procedure:
 *      net_se_rans
 *
 * Abstract:
 *      Perform operations on receiving a Single RAN Setup request.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx  - The network context to operate on
 *      buf  - Buffer to send
 *      size - Size of the buffer to send
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
net_se_rans(netctx * ctx, char * msg, int size)
{
        emage *  a = container_of(ctx, emage, net);
        emtask * t = task_alloc(TASK_TYPE_RAN_SETUP, 1, 0, 0, msg, size);

        if(!t) {
                return -1;
        }

        EMDBG(a, "Processing RAN Setup\n");

        return net_sched_task(a,t);
}

/* Procedure:
 *      net_se_ransice
 *
 * Abstract:
 *      Perform operations on receiving a Single RAN Slice request.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx  - The network context to operate on
 *      buf  - Buffer to send
 *      size - Size of the buffer to send
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
net_se_ranslice(netctx * ctx, char * msg, int size)
{
        emage *  a = container_of(ctx, emage, net);
        emtask * t = task_alloc(TASK_TYPE_RAN_SLICE, 1, 0, 0, msg, size);

        if(!t) {
                return -1;
        }

        EMDBG(a, "Processing RAN slice\n");

        return net_sched_task(a, t);
}

/* Procedure:
 *      net_te_ue_measure
 *
 * Abstract:
 *      Perform operations on receiving a Triggered UE Measurement request.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx  - The network context to operate on
 *      buf  - Buffer to send
 *      size - Size of the buffer to send
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
net_te_ue_measure(netctx * ctx, char * msg, int size)
{
        mod_id_t   mod;
        seq_id_t   seq;
        uint32_t   op;
        ep_ue_meas m;

        emtask * task;
        emtri *  trig;
        emage *  agent = container_of(ctx, emage, net);

        EMDBG(agent, "Processing UE measurement\n");

        epp_head(msg, size, 0, 0, 0, &mod, 0);

        seq = epp_seq(msg, size);
        op  = epp_trigger_op(msg, size);

        epp_trigger_uemeas_req(msg, size, &m);

        if(op != EP_OPERATION_ADD) {
                for(; m.nof_rrc > 0; m.nof_rrc--) {
                        trig_del_by_inst(
                                &agent->trig,
                                mod,
                                TR_TYPE_UE_MEAS,
                                (int)m.rrc[m.nof_rrc - 1].meas_id);
                }

                return 0;
        }

        for(; m.nof_rrc > 0; m.nof_rrc--) {
                trig = trig_alloc(
                        mod,
                        TR_TYPE_UE_MEAS,
                        (int)m.rrc[m.nof_rrc - 1].meas_id,
                        msg,
                        (unsigned int)size);

                if(!trig) {
                        EMLOG(agent, "Trigger creation failed!\n");
                        return 0;
                }

                trig_add(&agent->trig, trig);

                task = task_alloc(TASK_TYPE_UE_MEASURE, 1, 0, trig->id, 0, 0);

                if(!task) {
                        EMLOG(agent, "Cannot create task!\n");

                        trig_del_by_id(&agent->trig, trig->id);
                        return 0;
                }

                /* Scheduler this task, look for the next one */
                net_sched_task(agent, task);
        }

        return 0;
}

/* Procedure:
 *      net_te_ue_report
 *
 * Abstract:
 *      Perform operations on receiving a Triggered UE Report request.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx  - The network context to operate on
 *      buf  - Buffer to send
 *      size - Size of the buffer to send
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
net_te_ue_report(netctx * ctx, char * msg, int size)
{
        uint32_t mod;
        uint32_t seq;
        uint32_t op;

        emtri *  t;
        emtask*  s;
        emage *  a = container_of(ctx, emage, net);

        epp_head(msg, size, 0, 0, 0, &mod, 0);

        seq = epp_seq(msg, size);
        op  = epp_trigger_op(msg, size);

        EMDBG(a, "Processing UE Report\n");

        if(op == EP_OPERATION_ADD) {
                t = trig_alloc(mod, TR_TYPE_UE_REP, 0, msg, size);

                if(!t) {
                        EMLOG(a, "Trigger creation failed!\n");
                        return 0;
                }

                trig_add(&a->trig, t);
        } else {
                return trig_del_by_inst(&a->trig, mod, TR_TYPE_UE_REP, 0);
        }

        s = task_alloc(TASK_TYPE_UE_REPORT, 1, 0, t->id, 0, 0);

        if(!s) {
                EMLOG(a, "Cannot create task!\n");

                trig_del_by_id(&a->trig, t->id);
                return 0;
        }

        return net_sched_task(a, s);
}

/* Procedure:
 *      net_te_cell_meas_report
 *
 * Abstract:
 *      Perform operations on receiving a Triggered Cell measurement request.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx  - The network context to operate on
 *      buf  - Buffer to send
 *      size - Size of the buffer to send
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
net_te_cell_meas_report(netctx * ctx, char * msg, int size)
{
        uint32_t mod;
        uint32_t seq;
        uint32_t op;

        emtri *  t;
        emtask * s;
        emage *  a = container_of(ctx, emage, net);

        epp_head(msg, size, 0, 0, 0, &mod, 0);

        seq = epp_seq(msg, size);
        op  = epp_trigger_op(msg, size);

        EMDBG(a, "Processing MAC Report\n");

        if(op == EP_OPERATION_ADD) {
                t = trig_alloc(mod, TASK_TYPE_CELL_MEASURE, 0, msg, size);

                if(!t) {
                        EMLOG(a, "Trigger creation failed!\n");
                        return 0;
                }

                trig_add(&a->trig, t);
        } else {
                return trig_del_by_inst(&a->trig, mod, TR_TYPE_MAC_REP, 0);
        }

        s = task_alloc(TASK_TYPE_CELL_MEASURE, 1, 0, t->id, 0, 0);

        if(!s) {
                EMLOG(a, "Cannot create task!\n");

                trig_del_by_id(&a->trig, t->id);
                return 0;
        }

        return net_sched_task(a, s);
}

/* Procedure:
 *      net_sc_cell_meas_report
 *
 * Abstract:
 *      Perform operations on receiving a Scheduled Cell measurement request.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx  - The network context to operate on
 *      buf  - Buffer to send
 *      size - Size of the buffer to send
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
net_sc_cell_meas_report(netctx * ctx, char * msg, int size)
{
        emage *  a = container_of(ctx, emage, net);
        emtask * t = task_alloc(
                TASK_TYPE_CELL_MEASURE,
                epp_sched_interval(msg, size),
                -1, /* Continue to reschedule */
                0,
                msg,
                size);

        if(!t) {
                return -1;
        }

        EMDBG(a, "Processing RAN Setup\n");

        return net_sched_task(a, t);
}

/******************************************************************************
 * Top-level message handlers.                                                *
 ******************************************************************************/

/* Procedure:
 *      net_process_sched_event
 *
 * Abstract:
 *      Process an incoming scheduled event.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx  - The network context to operate on
 *      buf  - Buffer to send
 *      size - Size of the buffer to send
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
net_process_sched_event(netctx * ctx, char * msg, unsigned int size)
{
#ifdef EBUG
        emage * a = container_of(ctx, emage, net);
#endif
        ep_act_type s = epp_schedule_type(msg, size);

        if(s == EP_ACT_INVALID) {
                EMDBG(a, "Malformed schedule-event message received!\n");
                return -1;
        }

        switch(s) {
        case EP_ACT_HELLO:
                if(epp_dir(msg, size) == EP_HDR_FLAG_DIR_REP) {
                        return net_sc_hello(ctx, msg, size);
                }
                break;
        case EP_ACT_CELL_MEASURE:
                if(epp_dir(msg, size) == EP_HDR_FLAG_DIR_REQ) {
                        return net_sc_cell_meas_report(ctx, msg, size);
                }
                break;
        default:
                EMDBG(a, "Unknown schedule-event message, type=%d\n", s);
                break;
        }

        return 0;
}

/* Procedure:
 *      net_process_single_event
 *
 * Abstract:
 *      Process an incoming single event.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx  - The network context to operate on
 *      buf  - Buffer to send
 *      size - Size of the buffer to send
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
net_process_single_event(netctx * ctx, char * msg, unsigned int size)
{
#ifdef EBUG
        emage * a = container_of(ctx, emage, net);
#endif
        ep_act_type s = epp_single_type(msg, size);

        if(s == EP_ACT_INVALID) {
                EMDBG(a, "Malformed single-event message received!\n");
                return -1;
        }

        switch(s) {
        case EP_ACT_HELLO:
                /* Do nothing */
                break;
        case EP_ACT_ECAP:
                if(epp_dir(msg, size) == EP_HDR_FLAG_DIR_REQ) {
                        return net_se_enb_setup(ctx, msg, size);
                }
                break;
        case EP_ACT_HANDOVER:
                if(epp_dir(msg, size) == EP_HDR_FLAG_DIR_REQ) {
                        return net_se_ho(ctx, msg, size);
                }
                break;
        case EP_ACT_RAN_SETUP:
                if (epp_dir(msg, size) == EP_HDR_FLAG_DIR_REQ) {
                        return net_se_rans(ctx, msg, size);
                }
                break;
        case EP_ACT_RAN_SLICE:
                if (epp_dir(msg, size) == EP_HDR_FLAG_DIR_REQ) {
                        return net_se_ranslice(ctx, msg, size);
                }
                break;
        default:
                EMDBG(a, "Unknown single-event message, type=%d\n", s);
                break;
        }

        return 0;
}

/* Procedure:
 *      net_process_trigger_event
 *
 * Abstract:
 *      Process an incoming trigger event.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx  - The network context to operate on
 *      buf  - Buffer to send
 *      size - Size of the buffer to send
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
net_process_trigger_event(netctx * ctx, char * msg, unsigned int size)
{
#ifdef EBUG
        emage * a = container_of(ctx, emage, net);
#endif
        ep_act_type t = epp_trigger_type(msg, size);

        if(t == EP_ACT_INVALID) {
                EMDBG(a, "Malformed trigger-event message received!\n");
                return -1;
        }

        switch(t) {
        case EP_ACT_UE_REPORT:
                return net_te_ue_report(ctx, msg, size);
        case EP_ACT_UE_MEASURE:
                return net_te_ue_measure(ctx, msg, size);
        case EP_ACT_CELL_MEASURE:
                return net_te_cell_meas_report(ctx, msg, size);
        default:
                EMDBG(a, "Unknown trigger-event message, type=%d\n", t);
                break;
        }

        return 0;
}

/* Procedure:
 *      net_process_message
 *
 * Abstract:
 *      Process an incoming message.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx  - The network context to operate on
 *      buf  - Buffer to send
 *      size - Size of the buffer to send
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
net_process_message(netctx * ctx, char * msg, unsigned int size)
{
#ifdef EBUG
        emage * a = container_of(ctx, emage, net);
#endif
        ep_msg_type mt = epp_msg_type(msg, size);

        switch(mt) {
        /* Single events messages. */
        case EP_TYPE_SINGLE_MSG:
                return net_process_single_event(ctx, msg, size);
        /* Scheduled events messages. */
        case EP_TYPE_SCHEDULE_MSG:
                return net_process_sched_event(ctx, msg, size);
        /* Triggered events messages. */
        case EP_TYPE_TRIGGER_MSG:
                return net_process_trigger_event(ctx, msg, size);
        default:
                EMDBG(a, "Unknown message received, type=%d\n", mt);
                break;
        }

        return 0;
}

/******************************************************************************
 * Network listener logic.                                                    *
 ******************************************************************************/

/* Procedure:
 *      net_loop
 *
 * Abstract:
 *      Network logic main loop. This will maintains its own context using a
 *      personal synchronization routine. Network context keep analyzing
 *      incoming messages, and then depending on the type it performs a series
 *      of action to add tasks and create/remove triggers.
 *
 *      Communication is based on empower-enb-protocols.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      args - network context to operate on
 *
 * Returns:
 *      Always a null pointer; returns no value.
 */
INTERNAL
void *
net_loop(void * args)
{
        netctx * net = (netctx *)args;
#ifdef EBUG
        emage *  a = container_of(net, emage, net);
#endif
        int             op;
        int             bread;
        int             mlen  = 0;
        unsigned int    wi = net->interval;
        struct timespec wt = {0}; /* Wait time. */
        struct timespec wc = {0}; /* Wait time for reconnection. */
        struct timespec td = {0};

        /* Convert the wait interval in a timespec struct. */
        while(wi >= 1000) {
                wi -= 1000;
                wt.tv_sec += 1;
        }
        wt.tv_nsec = wi * 1000000;

        /* At least 1 second between re-connection attempts */
        wc.tv_sec  = 1 + wt.tv_sec;
        wc.tv_nsec = wt.tv_nsec;

        EMDBG(a, "Network loop starting, interval=%d ms\n", net->interval);

        while(1) {
next:
                if(net->status == NET_STATUS_NOT_CONNECTED) {
                        if(net->stop) {
                                goto stop;
                        }

                        if(net_connect_to_controller(net)) {
                                /* Relax the CPU and retry connection */
                                nanosleep(&wc, &td);
                                continue;
                        }

                        if(net_connected(net)) {
                                net_not_connected(net);
                                /* Relax the CPU and retry connection */
                                nanosleep(&wc, &td);
                                continue;
                        }
                }

                bread = 0;

                /* Continue until EP_HEADER_SIZE bytes have been collected */
                while(bread < EP_HEADER_SIZE) {
                        if(net->stop) {
                                goto stop;
                        }

                        op = net_recv(
                                net, net->buf + bread, EP_HEADER_SIZE - bread);

                        if(op <= 0) {
                                if(errno == EAGAIN) {
                                        /* Relax the CPU. */
                                        nanosleep(&wt, &td);
                                        continue;
                                }

                                net_not_connected(net);
                                goto next;
                        }

                        bread += op;
                }

                if(bread != EP_HEADER_SIZE) {
                        EMDBG(a, "Read %d bytes, but %ld to process!\n",
                                bread, EP_HEADER_SIZE);

                        net_not_connected(net);
                        continue;
                }

                mlen = epp_msg_length(net->buf, bread);

                EMDBG(a, "Collecting a message of size %d\n", mlen);

                //bread = 0;

                /* Continue until the entire message has been collected */
                while(bread < mlen) {
                        if(net->stop) {
                                goto stop;
                        }

                        op = net_recv(net, net->buf + bread, mlen - bread);

                        if(op <= 0) {
                                if(errno == EAGAIN) {
                                        /* Relax the CPU. */
                                        nanosleep(&wt, &td);
                                        continue;
                                }

                                net_not_connected(net);
                                goto next;
                                continue;
                        }

                        bread += op;
                }

                if(bread != mlen) {
                        EMDBG(a, "Read %d bytes out of %d\n",
                                bread, mlen);

                        net_not_connected(net);
                        //goto next;
                        continue;
                }

                /* Finally we collected the entire message; process it! */
                net_process_message(net, net->buf, bread);
        }

stop:
        EMDBG(a, "Network loop exiting...\n");

        /*
         * If you need to release 'net' specific resources, do it here!
         */

        return 0;
}

/* Procedure:
 *      net_start
 *
 * Abstract:
 *      Start a network threading context. When calling this a network thread
 *      can begin to operate, even before the call actually ends.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx - network context to operate on
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
net_start(netctx * ctx)
{
        emage * a = container_of(ctx, emage, net);

        EMDBG(a, "Initializing networking context\n");

        ctx->interval = NET_WAIT_TIME;
        ctx->sockfd   = -1;

        pthread_spin_init(&ctx->lock, 0);

        /* Create the context where the agent scheduler will run on. */
        if(pthread_create((pthread_t *)&ctx->thread, NULL, net_loop, ctx)) {
                EMLOG(a, "Failed to create the network thread\n");
                return -1;
        }

        return 0;
}

/* Procedure:
 *      net_stop
 *
 * Abstract:
 *      Stops a network thread. Invoking this call will make the caller wait for
 *      the thread to terminate. During thread termination all the context data
 *      will be released, so will not be safe anymore to access it from outside.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx - network context to operate on
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
net_stop(netctx * ctx)
{
#ifdef EBUG
        emage * a = container_of(ctx, emage, net);
#endif

        EMDBG(a, "Stopping networking context\n");

        /* Stop and wait for it... */
        ctx->stop = 1;
        pthread_join(ctx->thread, 0);

        pthread_spin_destroy(&ctx->lock);

        return 0;
}
