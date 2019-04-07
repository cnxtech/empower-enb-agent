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

#ifndef __EMAGE_LOG_H
#define __EMAGE_LOG_H

#include <stdio.h>

#include "visibility.h"

typedef struct __emage_agent emage;

/* Length dedicated for logging the procedure name */
#define EMLOG_PROC_LEN "32"

/* Log routine for every feedback */
#define EMLOG(a, x, ...)   log_message(a, __func__, x, ##__VA_ARGS__)

#ifdef EBUG
/* Debugging routine; valid only when EBUG symbol is defined  */
#define EMDBG(a, x, ...)   log_message(a, __func__, x, ##__VA_ARGS__)
#else /* EBUG */
#define EMDBG(a, x, ...)   /* Into nothing */
#endif /* EBUG */

/* Initialize the logging subsystem */
INTERNAL int  log_init();

/* Releases the logging subsystem and close any existing resource */
INTERNAL void log_release();

/* Log a message with a printf-like style inside a previously initialized file
 * on the file-system. This procedure should be called after 'log_init', but
 * don't generate expection if invoked before.
 */
INTERNAL void log_message(emage * agent, const char * proc, char * msg, ...);

#endif /* __EMAGE_LOG_H */
