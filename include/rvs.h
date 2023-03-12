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

#ifndef _RVS_H
#define _RVS_H

#include <rvif.h>

#define RVS_NUM_HASH_ENT (1024)
#define RVS_MAX_PORT (256)
#define RVS_LOCK_BUF_SIZE (256)

struct rvs {
	struct {
		char lock[RVS_LOCK_BUF_SIZE];
		struct {
			unsigned short port;
			unsigned long mac;
		} ent[RVS_NUM_HASH_ENT];
	} ft;

	char lock[RVS_LOCK_BUF_SIZE];

	struct {
		char lock[RVIF_MAX_QUEUE][RVS_LOCK_BUF_SIZE];
		struct rvif *vif;
	} port[RVS_MAX_PORT];
};

unsigned short rvs_fwd(struct rvs *, unsigned short, unsigned short, unsigned short);
int rvs_vif_attach(struct rvs *, unsigned short, struct rvif *);
int rvs_vif_detach(struct rvs *, unsigned short, struct rvif *);
int rvs_init(struct rvs *);
int rvs_exit(struct rvs *);

#endif
