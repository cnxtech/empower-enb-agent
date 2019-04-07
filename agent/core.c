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
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <pthread.h>

#include "agent.h"

/*
 *
 * Core elements
 *
 */

/* Variable:
 *      core_main_lock
 *
 * Abstract:
 *      Synchronization mechanism in charge of synchronize the core components
 *      allocation, initialization and removal. This element is used during
 *      agent starting operations.
 */
INTERNAL pthread_mutex_t core_main_lock = PTHREAD_MUTEX_INITIALIZER;

/* Variable:
 *      core_init
 *
 * Abstract:
 *      Boolean-like variable which register if initialization took place.
 */
INTERNAL int             core_init = 0;

/*
 *
 * Agents elements
 *
 */

/* Variable:
 *      core_agents_lock
 *
 * Abstract:
 *      Synchronization mechanism in charge of synchronize the addition and
 *      removal of agent instances into the core. This element is of course used
 *      also in every operation that works on agents.
 */
INTERNAL pthread_mutex_t core_agents_lock = PTHREAD_MUTEX_INITIALIZER;

/* Variable:
 *      core_agents
 *
 * Abstract:
 *      List of agents supported by the system.
 */
INTERNAL LIST_HEAD(core_agents);

/*
 *
 * Some procedures prototypes:
 *
 */

INTERNAL int core_initialize(void);
INTERNAL int core_send(emage * agent, char * msg, unsigned int size);
INTERNAL int core_release_agent(emage * agent);

/******************************************************************************
 * Atomic access for critical sections                                        *
 ******************************************************************************/

/* Procedure:
 *      init_lock
 *
 * Abstract:
 *      Perform locking over the core.
 *
 * Assumptions:
 *      Main lock already initialized.
 *
 * Arguments:
 *      ---
 *
 * Returns:
 *      ---
 */
#define core_lock()        pthread_mutex_lock(&core_main_lock)

/* Procedure:
 *      init_unlock
 *
 * Abstract:
 *      Perform unlocking over the core.
 *
 * Assumptions:
 *      Main lock already initialized.
 *
 * Arguments:
 *      ---
 *
 * Returns:
 *      ---
 */
#define core_unlock()      pthread_mutex_unlock(&core_main_lock)

/* Procedure:
 *      agents_lock
 *
 * Abstract:
 *      Perform locking over the agents set.
 *
 * Assumptions:
 *      Agent lock already initialized.
 *
 * Arguments:
 *      ---
 *
 * Returns:
 *      ---
 */
#define agents_lock()      pthread_mutex_lock(&core_agents_lock)

/* Procedure:
 *      agents_unlock
 *
 * Abstract:
 *      Perform unlocking over the agents set.
 *
 * Assumptions:
 *      Agent lock already initialized.
 *
 * Arguments:
 *      ---
 *
 * Returns:
 *      ---
 */
#define agents_unlock()    pthread_mutex_unlock(&core_agents_lock)

/******************************************************************************
 * Misc.                                                                      *
 ******************************************************************************/

/* Procedure:
 *      core_send
 *
 * Abstract:
 *      Perform the necessary steps to create a new "send" task into an agent.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      agent - The agent to operate on
 *      msg   - The message to send
 *      size  - Size of the message to send
 *
 * Returns:
 *      0 on success, otherwise a negative error value
 */
INTERNAL
int
core_send(emage * agent, char * msg, unsigned int size)
{
        char *   buf;
        emtask * task = task_alloc(TASK_TYPE_SEND, 1, 0, 0, msg, size);

        if(!task) {
                EMLOG(agent, "Cannot create new SEND task\n");
                return -1;
        }

        return sched_add_task(&agent->sched, task);
}

/* Procedure:
 *      core_initialize
 *
 * Abstract:
 *      Initialize the core systems of the library.
 *
 * Assumptions:
 *      Core lock held by the caller.
 *
 * Arguments:
 *      ---
 *
 * Returns:
 *      0 on success, otherwise a negative error value
 */
INTERNAL
int
core_initialize(void)
{
        if(core_init) {
                return 0;
        }

        /*
         *
         * Add here initialization/allocation that MUST be done before using the
         * core system starts. This will be done ONCE!
         *
         */

	/* Initialize the logging mechanism */
        log_init();

        core_init = 1;

        return 0;
}

/* Procedure:
 *      core_release_agent
 *
 * Abstract:
 *      Stops and release and agent instance. This procedure will also cause the
 *      release of all the agent resources.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ---
 *
 * Returns:
 *      0 on success, otherwise a negative error value
 */
INTERNAL
int
core_release_agent(emage * agent)
{
        sched_stop(&agent->sched);
        net_stop(&agent->net);
        trig_flush(&agent->trig);

        EMDBG(agent, "Agent released\n");

        /* Finally free the memory */
        free(agent);

        return 0;
}

/******************************************************************************
 * Public API implementation                                                  *
 ******************************************************************************/

/* Procedure:
 *      em_has_trigger
 *
 * Abstract:
 *      Check the existence of a trigger in a specific agent instance.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      enb_id - Id assigned to the eNB
 *      tid    - Trigger ID to look for
 *
 * Returns:
 *      Boolean style: 1 if trigger is present, 0 otherwise.
 */
EMAGE_API
int
em_has_trigger(uint64_t enb_id, int tid)
{
        emage * a = 0;
        emtri * t = 0;

/****** Start of the critical section *****************************************/
        agents_lock();

        list_for_each_entry(a, &core_agents, next) {
                if(a->enb_id == enb_id) {
                        t = trig_find_by_id(&a->trig, (trig_id_t)tid);
                        break;
                }
        }

        agents_unlock();
/****** End of the critical section *******************************************/

        return t ? 1 : 0;
}

/* Procedure:
 *      em_del_trigger
 *
 * Abstract:
 *      Removes a trigger form a specific agent instance.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      enb_id - Id assigned to the eNB
 *      tid    - Trigger ID to look for
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
EMAGE_API
int
em_del_trigger(uint64_t enb_id, int tid)
{
        emage * a = 0;
        emtri * t = 0;

/****** Start of the critical section *****************************************/
        agents_lock();

        list_for_each_entry(a, &core_agents, next) {
                if(a->enb_id == enb_id) {
                        trig_del_by_id(&a->trig, (trig_id_t)tid);
                        break;
                }
        }

        agents_unlock();
/****** End of the critical section *******************************************/

        return t ? 1 : 0;
}

/* Procedure:
 *      em_is_connected
 *
 * Abstract:
 *      Check if the network subsystem consider itself as connected with a
 *      controller.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      enb_id - Id assigned to the eNB
 *
 * Returns:
 *      Boolean style: 1 if connected, 0 otherwise.
 */
EMAGE_API
int
em_is_connected(uint64_t enb_id)
{
        emage * a = 0;
        int     f = 0;

/****** Start of the critical section *****************************************/
        agents_lock();

        list_for_each_entry(a, &core_agents, next) {
                if(a->enb_id == enb_id) {
                        f = (a->net.status == NET_STATUS_CONNECTED);
                        break;
                }
        }

        agents_unlock();
/****** End of the critical section *******************************************/

        return f;
}

/* Procedure:
 *      em_send
 *
 * Abstract:
 *      Send a message through an eNB agent active connection. The message must
 *      be conform to the protocol messages exchanged between controller and the
 *      agent itself. In case of malformed messages, the communication with the
 *      controller can be dropped.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      enb_id - Id assigned to the eNB
 *      msg    - The message to send
 *      size   - Size of the message
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
EMAGE_API
int
em_send(uint64_t enb_id, char * msg, unsigned int size)
{
        emage * a =  0;
        int     s = -1;

        /* Don't accept invalid buffers! */
        if(!msg) {
                return -1;
        }

        /* Don't accept 0 length messages! */
        if(!size) {
                return -1;
        }

/****** Start of the critical section *****************************************/
        agents_lock();

        list_for_each_entry(a, &core_agents, next) {
                if(a->enb_id == enb_id) {
                        s = core_send(a, msg, size);
                        break;
                }
        }

        agents_unlock();
/****** End of the critical section *******************************************/

        return s;
}

/* Procedure:
 *      em_terminate_agent
 *
 * Abstract:
 *      Stops and release an agent instance.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      enb_id - Id assigned to the eNB
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
EMAGE_API
int
em_terminate_agent(uint64_t enb_id)
{
        emage * a = 0;
        emage * b = 0;

        int     f = 0; /* Found? */
        int     s = 0; /* Status */

/****** Start of the critical section *****************************************/
        agents_lock();

        list_for_each_entry_safe(a, b, &core_agents, next) {
                if(a->enb_id == enb_id) {
                        list_del(&a->next);
                        f = 1;
                        break;
                }
        }

        agents_unlock();
/****** End of the critical section *******************************************/

        if(f) {
                if(a->ops && a->ops->release) {
                        a->ops->release();
                }

                core_release_agent(a);
        }

        return s;
}

/* Procedure:
 *      em_start
 *
 * Abstract:
 *      Initialize and starts a new agent instance.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      enb_id    - Id assigned to the eNB
 *      ops       - Custom operations of the callback wrapper
 *      ctrl_addr - Address of the controller to connect to
 *      ctrl_port - Port of the controller to connect to
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
EMAGE_API
int
em_start(
        uint64_t              enb_id,
        struct em_agent_ops * ops,
        char *                ctrl_addr,
        unsigned short        ctrl_port)
{
        emage * a  = 0;
        emage * f  = 0;

        int     s  = 0;  /* Status */
        int     nm = 0;  /* No mem? */

        if(!ops) {
                return -1;
        }

/****** Start of the critical section *****************************************/
        core_lock();
        core_initialize();
        core_unlock();
/****** End of the critical section *******************************************/

/****** Start of the critical section *****************************************/
        agents_lock();

        /* Find if an agent with the same id is already there */
        list_for_each_entry(f, &core_agents, next) {
                if(f->enb_id == enb_id) {
                        agents_unlock(); /* Unlocking *************************/

			EMLOG(0, "Agent eNB %d is already running...\n",
				enb_id);

			return -1;
                }
        }

        agents_unlock();
/****** End of the critical section *******************************************/

        a = malloc(sizeof(emage));

        if(!a) {
                return -1;
        }

        /* Clear agent memory */
        memset(a, 0, sizeof(emage));

        /* Initialize important elements*/
        INIT_LIST_HEAD(&a->next);
        a->enb_id = enb_id;
        memcpy(a->net.addr, ctrl_addr, strlen(ctrl_addr));
        a->net.port  = ctrl_port;
        a->ops       = ops;

        /*
         * Trigger subsystem initialization
         */

        a->trig.next = 1;
        pthread_spin_init(&a->trig.lock, 0);
        INIT_LIST_HEAD(&a->trig.ts);

        /*
         * Start this agent scheduler subsystem:
         */

        if(sched_start(&a->sched)) {
                EMLOG(a, "Failed to start the scheduler context\n");
                s = -1;

                goto err;
        }

        /*
         * Start this agent network subsystem:
         */

        if(net_start(&a->net)) {
                EMLOG(a, "Failed to start the network context\n");
                s = -1;

                goto err;
        }

        /* Custom initialization when everything seems ready */
        if (a->ops && a->ops->init) {
                /* Invoke custom initialization */
                s = a->ops->init();

                /* On error, do not launch the agent */
                if (s < 0) {
                        goto err;
                }
        }

        EMDBG(a, "Agent initialization finished\n");

/****** Start of the critical section *****************************************/
        agents_lock();
        list_add(&a->next, &core_agents);
        agents_unlock();
/****** End of the critical section *******************************************/

        return 0;

err:
        core_release_agent(a);
        return s;
}

/* Procedure:
 *      em_stop
 *
 * Abstract:
 *      Stop the entire core, releasing all the resources of the library. This
 *      procedure is quite extreme and can harm all the running agents.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *
 * Returns:
 *      0 on success, otherwise a negative error code
 */
EMAGE_API
int
em_stop(void)
{
        emage * a = 0;

        /* Loop until all the agents are released... */
        while(!list_empty(&core_agents)) {
/****** Start of the critical section *****************************************/
                agents_lock();

                a = list_first_entry(&core_agents, emage, next);
                list_del(&a->next);

                agents_unlock();
/****** End of the critical section *******************************************/

                if(a->ops && a->ops->release) {
                        a->ops->release();
                }

                core_release_agent(a);
        }

        EMLOG(0, "Core shut down\n");

        /* Close the logging utilities */
        log_release();

        return 0;
}
