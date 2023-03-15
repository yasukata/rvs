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

#include <rvs.h>

extern int rvs_lock_init(char *);
extern int rvs_lock_destroy(char *);
extern int rvs_wrlock(char *);
extern int rvs_wrunlock(char *);
extern int rvs_rdlock(char *);
extern int rvs_rdunlock(char *);

extern int rvs_notify(struct rvs *, unsigned short, unsigned short);

unsigned short rvs_fwd(struct rvs *vs, unsigned short vid, unsigned short qid, unsigned short batch)
{
	unsigned short cnt = 0;
	{
		rvs_rdlock(vs->lock);
		if (vs->port[vid].vif) {
			volatile unsigned short h = vs->port[vid].vif->queue[qid].ring[1].head;
			{
				unsigned short fwd[RVS_MAX_PORT + 1][RVIF_MAX_SLOT], fwd_cnt[RVS_MAX_PORT + 1] = { 0 };
				{
					volatile unsigned short t = vs->port[vid].vif->queue[qid].ring[1].tail;
					__asm__ volatile ("" ::: "memory");
					while (h != t && cnt < batch) {
						char *p = (char *)((unsigned long) vs->port[vid].vif + vs->port[vid].vif->queue[qid].ring[1].slot[h].off);
						{
							unsigned long s = (*((unsigned long *)(&p[4])) >> 16) & 0x0000ffffffffffff;
							vs->ft.ent[s % (RVS_NUM_HASH_ENT - 1)].port = vid;
							vs->ft.ent[s % (RVS_NUM_HASH_ENT - 1)].mac = s;
						}
						{
							unsigned short dst;
							{
								unsigned long d = *((unsigned long *)(&p[0])) & 0x0000ffffffffffff;
								if (vs->ft.ent[d % (RVS_NUM_HASH_ENT - 1)].mac == d)
									dst = vs->ft.ent[d % (RVS_NUM_HASH_ENT - 1)].port;
								else
									dst = RVS_MAX_PORT;
							}
							fwd[dst][fwd_cnt[dst]++] = h;
						}
						if (++h == vs->port[vid].vif->queue[qid].ring[1].num) h = 0;
						cnt++;
					}
				}
				{
					unsigned short i;
					for (i = 0; i < RVS_MAX_PORT; i++) {
						if (i != vid && vs->port[i].vif && vs->port[i].vif->num && (fwd_cnt[i] + fwd_cnt[RVS_MAX_PORT])) {
							rvs_wrlock(vs->port[i].lock[qid % vs->port[i].vif->num]);
							{
								volatile unsigned short d_h = vs->port[i].vif->queue[qid % vs->port[i].vif->num].ring[0].head, d_t = vs->port[i].vif->queue[qid % vs->port[i].vif->num].ring[0].tail;
								__asm__ volatile ("" ::: "memory");
								{
									unsigned short j;
									for (j = 0;
											(j < fwd_cnt[i] + fwd_cnt[RVS_MAX_PORT])
											&& ((d_h + 1 == vs->port[i].vif->queue[qid % vs->port[i].vif->num].ring[0].num ? 0 : d_h + 1) != d_t);
											j++, d_h = (d_h + 1 == vs->port[i].vif->queue[qid % vs->port[i].vif->num].ring[0].num ? 0 : d_h + 1)) {
										vs->port[i].vif->queue[qid % vs->port[i].vif->num].ring[0].slot[d_h].len = vs->port[vid].vif->queue[qid].ring[1].slot[fwd[j < fwd_cnt[i] ? i : RVS_MAX_PORT][j < fwd_cnt[i] ? j : j - fwd_cnt[i]]].len;
										{
											void *dst = (void *)(((unsigned long) vs->port[i].vif) + vs->port[i].vif->queue[qid % vs->port[i].vif->num].ring[0].slot[d_h].off);
											void *src = (void *)(((unsigned long) vs->port[vid].vif) + vs->port[vid].vif->queue[qid].ring[1].slot[fwd[j < fwd_cnt[i] ? i : RVS_MAX_PORT][j < fwd_cnt[i] ? j : j - fwd_cnt[i]]].off);
											unsigned short n = vs->port[i].vif->queue[qid % vs->port[i].vif->num].ring[0].slot[d_h].len;
											{
												unsigned short k;
												for (k = 0; k < ((n / 8) + 1); k++)
													((unsigned long *) dst)[k] = ((unsigned long *) src)[k];
											}
										}
									}
								}
								vs->port[i].vif->queue[qid % vs->port[i].vif->num].ring[0].head = d_h;
								rvs_notify(vs, i, qid % vs->port[i].vif->num);
							}
							rvs_wrunlock(vs->port[i].lock[qid % vs->port[i].vif->num]);
						}
					}
				}
			}
			__asm__ volatile ("" ::: "memory");
			vs->port[vid].vif->queue[qid].ring[1].head = h;
		}
		rvs_rdunlock(vs->lock);
	}
	return cnt;
}

int rvs_vif_attach(struct rvs *vs, unsigned short vid, struct rvif *vif)
{
	int ret = 0;
	rvs_wrlock(vs->lock);
	if (!vs->port[vid].vif)
		vs->port[vid].vif = vif;
	else
		ret = -1;
	rvs_wrunlock(vs->lock);
	return ret;
}

int rvs_vif_detach(struct rvs *vs, unsigned short vid, struct rvif *vif)
{
	int ret = 0;
	rvs_wrlock(vs->lock);
	if (vs->port[vid].vif == vif)
		vs->port[vid].vif = (void *) 0;
	else
		ret = -1;
	rvs_wrunlock(vs->lock);
	return ret;
}

int rvs_init(struct rvs *vs)
{
	{
		unsigned long i;
		for (i = 0; i < sizeof(struct rvs); i++)
			((char *) vs)[i] = 0;
	}
	rvs_lock_init(vs->lock);
	rvs_lock_init(vs->ft.lock);
	{
		unsigned short i;
		for (i = 0; i < RVS_MAX_PORT; i++) {
			{
				unsigned short j;
				for (j = 0; j < RVIF_MAX_QUEUE; j++)
					rvs_lock_init(vs->port[i].lock[j]);
			}
			vs->port[i].vif = (void *) 0;
		}
	}
	return 0;
}

int rvs_exit(struct rvs *vs)
{
	rvs_lock_destroy(vs->lock);
	rvs_lock_destroy(vs->ft.lock);
	{
		unsigned short i;
		for (i = 0; i < RVS_MAX_PORT; i++) {
			{
				unsigned short j;
				for (j = 0; j < RVIF_MAX_QUEUE; j++)
					rvs_lock_destroy(vs->port[i].lock[j]);
			}
		}
	}
	return 0;
}
