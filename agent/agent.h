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

#ifndef __EMAGE_AGENT_H
#define __EMAGE_AGENT_H

#include <emage.h>
#include <emage/emproto.h>

#include "emlist.h"
#include "emtypes.h"
#include "log.h"
#include "net.h"
#include "sched.h"
#include "task.h"
#include "triggers.h"
#include "visibility.h"

/* See emage.h public header for more info on this... */
struct em_agent_ops;

/* Structure:
 *      emage
 *
 * Abstract:
 *      Provides the context of an agent. This structure organize the agent data
 *      and state machines.
 *
 * Critical sections:
 *      The insertion and removal of this structure is synchronized by core_lock
 *      mutex. Every sybsystem has its own lock to grant a flexible and
 *      independent usage of the resources.
 */
typedef struct __emage_agent {
        /* Member of a list (there are more than one agent) */
        struct list_head      next;

        /* eNB ID bound to this agent context */
        enb_id_t              enb_id;

        /* Operations related to the agent */
        struct em_agent_ops * ops;

        /* Context containing the active triggers of this agent */
        trctx                 trig;
        /* Context containing the network state machine */
        netctx                net;
        /* Context containing the state machine to run tasks in time */
        schctx                sched;
} emage;

#endif /* __EMAGE_AGENT_H */
