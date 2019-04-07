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

#ifndef __EMAGE_VISIBILITY_H
#define __EMAGE_VISIBILITY_H

/* The symbol will be visible and usable by external modules */
#define EMAGE_API       __attribute__ ((visibility ("default")))

/* The symbol will be used only within this library and will not be exported */
#define INTERNAL        __attribute__ ((visibility ("hidden")))

#endif /* __EMAGE_VISIBILITY_H */
