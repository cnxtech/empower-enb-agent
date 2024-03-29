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

/*
 * Empower Agent.
 */

#ifndef __EMAGE_H
#define __EMAGE_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <stdint.h>

#include <emage/emproto.h>

/*
 * Layer 2 RAN sharing configuration
 */
typedef struct em_RAN_l2_configuration {
        /* User scheduler requested */
        int user_sched;
        /* RBGs associated with the slice */
        int rbg;
} em_RAN_l2_conf;

/*
 * RAN sharing configuration for a specific slice
 */
typedef struct em_RAN_configuration {
        int            nof_users;
        /* Array of users of the slice */
        uint16_t       users[EP_RAN_USERS_MAX];
        /* Layer 2 configuration */
        em_RAN_l2_conf l2;
} em_RAN_conf;

/*
 * Radio Access Network(RAN) sharing related procedures:
 */
struct em_RAN_ops {
        /* Informs the wrapper that the controller requested to report the
         * status of the RAN mechanism.
         *
         * Returns 0 on success, a negative error code otherwise.
         */
        int (* setup_request) (uint32_t mod);

        /* Informs the wrapper that the controller requested to report the
         * current status of a specific slice.
         *
         * Returns 0 on success, a negative error code otherwise.
         */
        int (* slice_request) (uint32_t mod, uint64_t slice);

        /* Informs the wrapper that the controller requested to add a new slice
         * within the RAN subsystem. During the association also the slice
         * scheduler for its users is specified.
         *
         * Returns 0 on success, a negative error code otherwise.
         */
        int (* slice_add) (
                uint32_t mod, uint64_t slice, em_RAN_conf * det);

        /* Informs the wrapper that the controller requested to remove an
         * existing slice from RAN subsystem.
         *
         * Returns 0 on success, a negative error code otherwise.
         */
        int (* slice_rem) (uint32_t mod, uint64_t slice);

        /* Informs the wrapper that the controller requested to configure a
         * slice in a specific way.
         *
         * Returns 0 on success, a negative error code otherwise.
         */
        int (* slice_conf) (uint32_t mod, uint64_t slice, em_RAN_conf * conf);
};

/* Defines the operations that can be customized depending on the technology
 * where you want to embed the agent to. Such procedures will be called by the
 * agent main logic while responding to the controller orders or events
 * triggered by the local system.
 */
struct em_agent_ops {
        /* Perform custom initialization for the technology abstraction layer.
         *
         * Reporting an error during initialization stages cause the agent to
         * fail.
         *
         * Returns 0 on success, a negative error code otherwise.
         */
        int (* init) (void);

        /* Perform custom releasing for the technology abstraction layer.
         * Regardless of error returns codes, the agent will be stopped.
         *
         * Returns 0 on success, a negative error code otherwise.
         */
        int (* release) (void);

        /* Signal the wrapper that the controller disconnected from the agent.
         *
         * You should take the necessary operation to ensure a coherent behavior
         * between the two instances.
         *
         * Returns 0 on success, a negative error code otherwise.
         */
        int (* disconnected) (void);

        /*
         * Setup messages:
         */

        /* The controller requested the current setup of the base station.
         *
         * Returns 0 on success, a negative error code otherwise.
         */
        int (* enb_setup_request) (uint32_t mod);

        /*
         * eNB Capabilities-related procedures:
         */

        /* Informs the stack that a log for UE activity has been required by the
         * controller. The wrapper shall perform operations to enable such
         * functionality into the base station.
         *
         * 'mod' represent the ctrl module which requested for such report.
         * Trigger id has to be used to check for its existence later.
         *
         * Returns 0 on success, a negative error code otherwise.
         */
        int (* ue_report) (uint32_t mod, int trig_id);

        /* Informs the stack that a measurement request has been issued by the
         * controller for a certain device.
         *
         * Returns 0 on success, a negative error code otherwise.
         */
        int (* ue_measure) (
                uint32_t     mod,
                int          trig_id,
                uint8_t      measure_id,
                uint16_t     rnti,
                uint16_t     earfcn,
                uint16_t     interval,
                int16_t      max_cells,
                int16_t      max_meas);

        /* Informs the stack that an Handover command has been requested by the
         * controller. The request is directed for this eNB to a target one,
         * with a defined transaction from a cell to another one.
         *
         * Returns 0 on success, a negative error code otherwise.
         */
        int (* handover_UE) (
                uint32_t mod,
                uint16_t source_cell,
                uint16_t rnti,
                uint64_t target_enb,
                uint16_t target_cell,
                uint8_t  cause);

        /*
         * Cell-related capabilities procedures:
         */

        /* Informs the wrapper that the controller required to report the status
         * of the MAC layer.
         *
         * Returns 0 on success, a negative error code otherwise.
         */
        int (* cell_measure) (
                uint16_t cell,
                uint32_t mod,
                int32_t  interval,
                int      trig_id);

        /* Radio Access Network operations that allows to customize the slicing
         * within the wrapper logic.
         */
        struct em_RAN_ops ran;
};

/* Peek the triggers of the given agent and check if a trigger is enabled or
 * not. This is useful to avoid doing some heavy operation and just being denied
 * at the end.
 *
 * Returns 1 if the trigger is enabled, 0 otherwise.
 */
int em_has_trigger(uint64_t enb_id, int tid);

/* Peek the triggers of the given agent and remove a well identified one.
 *
 * Returns 0 if the trigger has been removed, a negative error code otherwise.
 */
int em_del_trigger(uint64_t enb_id, int tid);

/* Check if the agent is currently connected to a controller.
 *
 * Returns 1 if the agent is connected to a controller, 0 otherwise.
 */
int em_is_connected(uint64_t enb_id);

/* Send a message to the connected controller, if any controller is attached.
 * This operations is only possible if the agent for that particular id has
 * already been created.
 *
 * Returns 0 if the message is successfully sent, a negative error code
 * otherwise.
 */
int em_send(uint64_t enb_id, char * msg, unsigned int size);

/* Start the Empower Agent logic. This will cause the agent to start interacting
 * with a remote controller or to local events. You need to pass the technology
 * dependent callbacks and the base station identifier.
 *
 * Returns 0 on success, or a negative error code on failure.
 */
int em_start(
        uint64_t              b_id,
        struct em_agent_ops * ops,
        char *                ctrl_addr,
        unsigned short        ctrl_port);

/* Stop the Empower Agent logic. This will cause the agent to stop to all the
 * controller commands and local events.
 *
 * Returns 0 on success, or a negative error code on failure.
 */
int em_stop(void);

/* Terminate a single agent instance using its ID.
 *
 * Always returns 0.
 */
int em_terminate_agent(uint64_t b_id);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EMAGE_H */
