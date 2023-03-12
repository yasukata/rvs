/*
 *
 * Copyright 2023 Kenichi Yasukata
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef _RVIF_H
#define _RVIF_H

#define RVIF_MAX_QUEUE (128)
#define RVIF_MAX_SLOT (1024)

struct rvif {
	unsigned long flags;
	unsigned int num;
	struct {
		unsigned long flags;
		struct {
			unsigned long flags;
			unsigned short num;
			unsigned short head;
			unsigned short tail;
			struct {
				unsigned long flags;
				unsigned long off;
				unsigned short len;
			} slot[RVIF_MAX_SLOT];
		} ring[2];
	} queue[RVIF_MAX_QUEUE];
};

#endif
