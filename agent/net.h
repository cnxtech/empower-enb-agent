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

#ifndef __EMAGE_NET_H
#define __EMAGE_NET_H

#include <pthread.h>

#include "visibility.h"

/* Not connected to the controller */
#define NET_STATUS_NOT_CONNECTED        0
/* Connected to the controller */
#define NET_STATUS_CONNECTED            1

/* Default buffer size */
#define EM_BUF_SIZE                     4096

/* Structure:
 *      netctx
 *
 * Abstract:
 *      Provides a description of how the network state machine organize its
 *      data and variables.
 *
 * Critical sections:
 *      The structure must be protected during initialization and release
 *      stages, and provides necessary synchronization primitives for network
 *      procedure to operate correctly.
 */
typedef struct __emage_netctx {
        /* Address to listen */
        char               addr[16];
        /* Port to listen. */
        unsigned short     port;
        /* Socket fd used for communication */
        int                sockfd;

        /* A value different than 0 stop this listener */
        int                stop;
        /* Status of the listener */
        int                status;
        /* Sequence number; can potentially overflow */
        unsigned int       seq;

        /* Buffer where message received are dumped.
         * This is used to avoid using too much stack area.
         */
        char               buf[EM_BUF_SIZE];

        /* Thread in charge of this listening */
        pthread_t          thread;
        /* Lock for elements of this context */
        pthread_spinlock_t lock;
        /* Time to wait at the end of each loop, in ms */
        unsigned int       interval;
} netctx;

/* Get the next valid sequence number to emit with this context */
INTERNAL seq_id_t net_next_seq(netctx * net);

/* Adjust the context due a network error */
INTERNAL int      net_not_connected(netctx * net);

/* Send a generic message using the network listener logic */
INTERNAL int      net_send(netctx * net, char * buf, unsigned int size);

/* Start a new listener in a different threading context */
INTERNAL int      net_start(netctx * net);

/* Order the network listener to stop it's operations */
INTERNAL int      net_stop(netctx * net);

#endif /* __EMAGE_NET_H */
