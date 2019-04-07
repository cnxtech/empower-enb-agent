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

#ifndef __EMAGE_TYPES_H
#define __EMAGE_TYPES_H

#include <stdint.h>

/* This will include types from protocols, like eNB and module IDs */
#include <emage/emproto.h>

typedef uint32_t     seq_id_t;      /* Sequence ID type */

/*
 *
 * Tasks:
 *
 */

typedef unsigned int task_id_t;     /* Task ID type */

/*
 *
 * Triggers:
 *
 */

typedef int          trig_id_t;     /* Trigger ID type */

#endif /* __EMAGE_TYPES_H */
