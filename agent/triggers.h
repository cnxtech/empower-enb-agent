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

#ifndef __EMAGE_TRIGGERS_H
#define __EMAGE_TRIGGERS_H

#include <pthread.h>

#include "emlist.h"
#include "visibility.h"

/* Possible type of triggers which can be created */
enum trigger_type {
        TR_TYPE_UE_REP,   /* UE report */
        TR_TYPE_UE_MEAS,  /* UE measurement */
        TR_TYPE_MAC_REP   /* MAC reporting */
};

/* Structure:
 *      emtri
 *
 * Abstract:
 *      Provides a description of what is a trigger. Trigger contains necessary
 *      elements to replicate the message and the module which issued it without
 *      being in the original message context.
 *
 * Critical sections:
 *      ---
 */
typedef struct __emage_trigger {
        /* Member of a list. */
        struct list_head next;

        /* Id of this trigger. */
        trig_id_t        id;
        /* Type of trigger */
        int              type;
        /* Module bound with this trigger. */
        mod_id_t         mod;
        /* Id bound to the instance for this trigger; this is useful to
         * distinguish between IDs of the same module.
         */
        int              instance;

        /* Original request message. */
        char *           msg;
        /* Size of the original request */
        unsigned int     msg_size;
} emtri;

/* Structure:
 *      trctx
 *
 * Abstract:
 *      Context for triggers associated with an agent. Contains the necessary
 *      data to handle multiple triggers at once.
 *
 * Critical sections:
 *      Initialization and release shoud be performed within caller critical
 *      section.
 */
typedef struct __emage_trigger_context {
        /* List of triggers. */
        struct list_head   ts;

        /* Id for the next trigger*/
        int                next;

        /* Lock for this context. */
        pthread_spinlock_t lock;
} trctx;

/* Allocates and initializes a new trigger using given arguments */
INTERNAL emtri *   trig_alloc(
        mod_id_t mod, int type, int instance, char * msg, unsigned int size);

/* Add a new trigger in the agent triggering context */
INTERNAL trig_id_t trig_add(trctx * ctx, emtri * trig);

/* Find, remove and free a trigger using its ID */
INTERNAL int       trig_del_by_id(trctx * tc, int id);

/* Find, remove and free a trigger using type, module and instance */
INTERNAL int       trig_del_by_inst(trctx * tc, int mod, int type, int instance);

/* Find an trigger with given id */
INTERNAL emtri *   trig_find_by_id(trctx * tc, int id);

/* Peek the context to see if it has trigger with specific keys */
INTERNAL emtri *   trig_find_by_inst(
       trctx * tc, int mod, int type, int instance);

/* Flush everything and clean the context. */
INTERNAL void      trig_flush(trctx * tc);

/* Free the resources of a trigger */
INTERNAL void      trig_free(emtri * tc);

/* Acquires the next usable trigger id */
INTERNAL int       trig_next_id(trctx * tc);

#endif
