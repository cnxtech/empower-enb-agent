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

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <emage/emproto.h>

#include "agent.h"

/* Very minimal memory reporting mechanism */
#ifdef EBUG_MEMORY

/* Total amount of memory used in trigger creation */
unsigned int trig_mem    = 0;
/* Memory has overflown? (unlikely, but still register it) */
int          trig_mem_OF = 0;

#endif /* EBUG_MEMORY */

/******************************************************************************
 * Locking and atomic context                                                 *
 ******************************************************************************/

/* Procedure:
 *      trig_lock_ctx
 *
 * Abstract:
 *      Lock a trigger context.
 *
 * Assumptions:
 *      Lock already initialized.
 *
 * Arguments:
 *      t - The trigger context to lock
 *
 * Returns:
 *      ---
 */
#define trig_lock_ctx(t)        pthread_spin_lock(&t->lock)

/* Procedure:
 *      trig_unlock_ctx
 *
 * Abstract:
 *      Unlock a trigger context.
 *
 * Assumptions:
 *      Lock already initialized.
 *
 * Arguments:
 *      t - The trigger context to unlock
 *
 * Returns:
 *      ---
 */
#define trig_unlock_ctx(t)      pthread_spin_unlock(&t->lock)

/******************************************************************************
 * Procedures                                                                 *
 ******************************************************************************/

/* Procedure:
 *      trig_alloc
 *
 * Abstract:
 *      Allocate a new trigger with the given arguments. This just reserve space
 *         and initialize the trigger, but does not add it to a context.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      mod      - Module which requested the trigger
 *      type     - Type of trigger
 *      instance - Instance ID (several module can request multiple triggers)
 *      msg      - Original message
 *      size     - Original message size
 *
 * Returns:
 *      Valid pointer to a trigger, otherwise a null pointer on error.
 */
INTERNAL
emtri *
trig_alloc(mod_id_t mod, int type, int instance, char * msg, unsigned int size)
{
        emtri * ret = malloc(sizeof(emtri));

        if(!ret) {
                return 0;
        }

        memset(ret, 0, sizeof(emtri));

        INIT_LIST_HEAD(&ret->next);
        ret->mod      = mod;
        ret->type     = type;
        ret->instance = instance;

        if(msg) {
                ret->msg = malloc(size);

                if(!ret->msg) {
                        free(ret);
                        return 0;
                }

                memcpy(ret->msg, msg, size);
                ret->msg_size = size;
        }

#ifdef EBUG_MEMORY
        /* Memory overflow, which means more than 4GB just for this! */
        if(trig_mem + sizeof(emtri) + size < trig_mem) {
                trig_mem_OF++;
        }

        trig_mem += sizeof(emtri) + size;

        printf("Triggers used memory is %09d bytes; overflow? %d\n",
                trig_mem,
                trig_mem_OF);
#endif  /* EBUG_MEMORY */

        return ret;
}

/* Procedure:
 *      trig_add
 *
 * Abstract:
 *      Adds a trigger to a trigger context.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx  - Context to operate on
 *      trig - Trigger to add to the context
 *
 * Returns:
 *      The ID assigned to the trigger, or a negative error value.
 */
INTERNAL
trig_id_t
trig_add(trctx * ctx, emtri * trig)
{
#ifdef EBUG
        emage * a = container_of(ctx, emage, trig);
#endif
/****** Start of the critical section *****************************************/
        trig_lock_ctx(ctx);
        trig->id = ctx->next++;
        list_add(&trig->next, &ctx->ts);
        trig_unlock_ctx(ctx);
/****** End of the critical section *******************************************/

        EMDBG(a, "New trigger added, id=%d, type=%d\n", trig->id, trig->type);

        return trig->id;
}

/* Procedure:
 *      trig_del_by_id
 *
 * Abstract:
 *      Removes a trigger from a trigger context. The search is performed using
 *      the ID originally assigned to the trigger.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx  - Context to operate on
 *      trig - Trigger to remove from the context
 *
 * Returns:
 *      0 on success, otherwise a negative error value
 */
INTERNAL
int
trig_del_by_id(trctx * ctx, trig_id_t id)
{
#ifdef EBUG
        emage * a = container_of(ctx, emage, trig);
#endif
        emtri * t = 0;
        emtri * u = 0;

/****** Start of the critical section *****************************************/
        trig_lock_ctx(ctx);

        list_for_each_entry_safe(t, u, &ctx->ts, next) {
                if(t->id == id) {
                        list_del(&t->next);
                        trig_unlock_ctx(ctx); /* Unlocking ********************/

                        EMDBG(a, "Removed trigger %d, type=%d\n",
				t->id, t->type);

                        trig_free(t);

                        return 0;
                }
        }

        trig_unlock_ctx(ctx);
/****** End of the critical section *******************************************/

        EMDBG(a, "Trigger %d not found!\n", id);

        return -1;
}

/* Procedure:
 *      trig_del_by_inst
 *
 * Abstract:
 *      Removes a trigger from a trigger context. The search is performed using
 *      IDs alternative to the unique one used.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx      - Context to operate on
 *      mod      - Controller module ID
 *      type     - Type of trigger
 *      instance - Instance of the trigger
 *
 * Returns:
 *      0 on success, otherwise a negative error value
 */
INTERNAL
int
trig_del_by_inst(trctx * ctx, int mod, int type, int instance)
{
#ifdef EBUG
        emage * a = container_of(ctx, emage, trig);
#endif
        emtri * t = 0;
        emtri * u = 0;

/****** Start of the critical section *****************************************/
        trig_lock_ctx(ctx);

        list_for_each_entry_safe(t, u, &ctx->ts, next) {
                if(t->type == type &&
                        t->mod == mod &&
                        t->instance == instance)
		{
                        list_del(&t->next);
                        trig_unlock_ctx(ctx); /* Unlocking ********************/

                        EMDBG(a, "Removed trigger %d, type=%d\n",
                                t->id, t->type);

                        trig_free(t);

                        return 0;
                }
        }

        trig_unlock_ctx(ctx);
/****** End of the critical section *******************************************/

        EMDBG(a, "Trigger not found; type=%d, mod=%d, instance=%d\n",
               type, mod, instance);

        return -1;
}

/* Procedure:
 *      trig_find_by_id
 *
 * Abstract:
 *      Find a certain trigger using it's ID. Operation is performed on a
 *      particular given context.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx - Context to operate on
 *      id  - Trigger ID to look for
 *
 * Returns:
 *      A trigger pointer on success, otherwise a null pointer.
 */
INTERNAL
emtri *
trig_find_by_id(trctx * ctx, int id)
{
        emtri * t = 0;

/****** Start of the critical section *****************************************/
        trig_lock_ctx(ctx);

        list_for_each_entry(t, &ctx->ts, next) {
                if(t->id == id) {
                        trig_unlock_ctx(ctx); /* Unlocking ********************/
                        return t;
                }
        }

        trig_unlock_ctx(ctx);
/****** End of the critical section *******************************************/

        return 0;
}

/* Procedure:
 *      trig_find_by_inst
 *
 * Abstract:
 *      Find a certain trigger using the alternative IDs. Operation is performed
 *      on a particular given context.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx      - Context to operate on
 *      mod      - Controller module ID
 *      type     - Type of trigger
 *      instance - Instance of the trigger
 *
 * Returns:
 *      A trigger pointer on success, otherwise a null pointer.
 */
INTERNAL
emtri *
trig_find_by_inst(trctx * ctx, int mod, int type, int instance)
{
        emtri * t = 0;

/****** Start of the critical section *****************************************/
        trig_lock_ctx(ctx);

        list_for_each_entry(t, &ctx->ts, next) {
                if(t->mod == mod &&
                        t->type == type &&
                        t->instance == instance)
		{
                        trig_unlock_ctx(ctx); /* Unlocking ********************/
			return t;
                }
        }

        trig_unlock_ctx(ctx);
/****** End of the critical section *******************************************/

        return 0;
}

/* Procedure:
 *      trig_flush
 *
 * Abstract:
 *      Flushes all the triggers present in a given context.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ctx - Context to operate on
 *
 * Returns:
 *      ---
 */
INTERNAL
void
trig_flush(trctx * ctx)
{
#ifdef EBUG
        emage * a = container_of(ctx, emage, trig);
#endif
        emtri * t = 0;
        emtri * u = 0;

	EMDBG(a, "Flushing triggers\n");

/****** Start of the critical section *****************************************/
        trig_lock_ctx(ctx);

        list_for_each_entry_safe(t, u, &ctx->ts, next) {
                EMDBG(a, "Flushing out trigger %d\n", t->id);

                list_del(&t->next);
                trig_free(t);
        }

        trig_unlock_ctx(ctx);
/****** End of the critical section *******************************************/

        return;
}

/* Procedure:
 *      trig_free
 *
 * Abstract:
 *      Free a previsously allocated trigger.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      trig - Trigger to be released
 *
 * Returns:
 *      ---
 */
INTERNAL
void
trig_free(emtri * trig)
{
        if(trig) {
#ifdef EBUG_MEMORY
                trig_mem -= sizeof(emtri) + trig->msg_size;

                printf("Triggers used memory is %09d bytes; overflow? %d\n",
                        trig_mem,
                        trig_mem_OF);
#endif  /* EBUG_MEMORY */

                if(trig->msg) {
                        free(trig->msg);
                        trig->msg = 0;
                        trig->msg_size = 0;
                }

                free(trig);
        }
}
