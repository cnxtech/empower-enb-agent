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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "agent.h"

/* Variable:
 *      log_fd
 *
 * Abstract:
 *      Logging file descriptor for all the library log messages.
 */
INTERNAL FILE * log_fd = 0;

/* Procedure:
 *      log_init
 *
 * Abstract:
 *      Initializes the logging subsystem. After this call the invoked logging
 *      traces will be visible in the log file.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ---
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
int
log_init()
{
        char lp[256] = {0};

        /* Unique log per process */
        sprintf(lp, "./emage.%d.log", getpid());
        log_fd = fopen(lp, "w");

        return 0;
}

/* Procedure:
 *      log_release
 *
 * Abstract:
 *      Releases the logging subsystem and free the associated resources.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      ---
 *
 * Returns:
 *      ---
 */
INTERNAL
void
log_release()
{
        if(log_fd) {
                fflush(log_fd);
                fclose(log_fd);
                log_fd = 0;
        }
}

/* Procedure:
 *      log_message
 *
 * Abstract:
 *      Logs a message in 'printf' format. The message will be marked with the
 *      eNB ID, if an eNB pointer is present, otherwise the message is organized
 *      with level zero.
 *
 * Assumptions:
 *      ---
 *
 * Arguments:
 *      agent - Agent subsystem which wants to log
 *      proc  - Procedure name which is performing the log request
 *      msg   - String in printf format
 *      ...   - Variable number of arguments
 *
 * Returns:
 *      0 on success, otherwise a negative error code.
 */
INTERNAL
void
log_message(emage * agent, const char * proc, char * msg, ...)
{
        va_list vl;

        /* First print the name of the procedure generating the log */
        fprintf(log_fd, "%-" EMLOG_PROC_LEN "s, eNB %03lu: ",
                proc, agent ? agent->enb_id : 0);

        va_start(vl, msg);
        vfprintf(log_fd, msg, vl);
        va_end(vl);

        return;
}
