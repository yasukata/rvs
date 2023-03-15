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

#include <rvif.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#include <sys/stat.h>
#include <sys/mman.h>

#include <arpa/inet.h>

#include <pthread.h>

static short pkt_len = 64;
static int mode_rx = 1;
static struct rvif *vif = NULL;

static _Atomic unsigned char global_counter_id = 0;
static _Atomic unsigned long global_pkt_cnt[2] = { 0 };
static _Atomic unsigned long global_pkt_byte[2] = { 0 };

struct udp_pseudo {
	unsigned int src;
	unsigned int dst;
	unsigned char pad;
	unsigned char proto;
	unsigned short len;
} __attribute__((packed));

struct udpkt {
	struct {
		unsigned char dst[6];
		unsigned char src[6];
		unsigned short type_be;
	} eth __attribute__((packed));
	struct {
		unsigned char l:4, v:4; /* LITTLE ENDIAN: the unit of l is 4 octet */
		//unsigned char v:4, l:4; /* BIG ENDIAN the unit of l is 4 octet */
		unsigned char tos;
		unsigned short len_be;
		unsigned short id_be;
		unsigned short off_be;
		unsigned char ttl;
		unsigned char proto;
		unsigned short csum_be;
		unsigned int src_be;
		unsigned int dst_be;
	} ip4 __attribute__((packed));
	struct {
		unsigned short src_be;
		unsigned short dst_be;
		unsigned short len_be;
		unsigned short csum_be;
	} udp __attribute__((packed));
	unsigned char payload[0];
} __attribute__((packed));

struct bchain {
	char *b;
	unsigned short l;
	struct bchain *next;
};

#define net_csum16(_b, _m) \
({ \
	unsigned int _r = 0, _k = 0; \
	struct bchain *_x; \
	for (_x = (_b); _x; _x = _x->next) { \
		unsigned int _i = 0; \
		for (_i = 0; _i < (_x->l); _i++, _k++) { \
			unsigned short _v = ((unsigned char *)(_x->b))[_i] & 0x00ff; \
			if (_k % 2 == 0) { \
				_v <<= 8; \
			} \
			_r += _v; \
		} \
	} \
	_r -= (_m); \
	_r = (_r >> 16) + (_r & 0x0000ffff); \
	_r = (_r >> 16) + (_r & 0x0000ffff); \
	(unsigned short)~((unsigned short) _r); \
})

static void *pktgen_fn(void *data)
{
	unsigned long qid = (unsigned long) data;
	{ /* xmit a packet for learning bridge */
		volatile unsigned short h, t;
		h = vif->queue[qid].ring[1].head;
		asm volatile ("" ::: "memory");
		t = vif->queue[qid].ring[1].tail;
		assert((t + 1 == vif->queue[qid].ring[1].num ? 0 : t + 1) != h);
		if (++t == vif->queue[qid].ring[1].num) t = 0;
		asm volatile ("" ::: "memory");
		vif->queue[qid].ring[1].tail = t;
	}
	while (1) {
		unsigned long pkt_cnt = 0, pkt_byte = 0;
		if (mode_rx) {
			volatile unsigned short h, t;
			h = vif->queue[qid].ring[0].head;
			asm volatile ("" ::: "memory");
			t = vif->queue[qid].ring[0].tail;
			while (t != h) {
				pkt_byte += vif->queue[qid].ring[0].slot[t].len;
				pkt_cnt++;
				if (++t == vif->queue[qid].ring[0].num) t = 0;
			}
			asm volatile ("" ::: "memory");
			vif->queue[qid].ring[0].tail = t;
		} else {
			volatile unsigned short h, t;
			h = vif->queue[qid].ring[1].head;
			asm volatile ("" ::: "memory");
			t = vif->queue[qid].ring[1].tail;
			while ((t + 1 == vif->queue[qid].ring[1].num ? 0 : t + 1) != h) {
				vif->queue[qid].ring[1].slot[t].len = pkt_len;
				pkt_byte += vif->queue[qid].ring[1].slot[t].len;
				pkt_cnt++;
				if (++t == vif->queue[qid].ring[1].num) t = 0;
			}
			asm volatile ("" ::: "memory");
			vif->queue[qid].ring[1].tail = t;
		}
		{
			volatile unsigned char counter_id = global_counter_id;
			asm volatile ("" ::: "memory");
			global_pkt_cnt[counter_id] += pkt_cnt;
			global_pkt_byte[counter_id] += pkt_byte;
		}
	}
	pthread_exit(NULL);
}

int main(int argc, char *const *argv)
{
	unsigned long mem_size = 0;
	unsigned int num_thread = 1;
	unsigned short num_slot = 1024;

	char mac_src[6] = { 0 }, mac_dst[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, };
	short udp_src_port = 12345, udp_dst_port = 23456;
	int src_ip4, dst_ip4;

	{
		inet_pton(AF_INET, "192.168.123.2", &src_ip4);
		inet_pton(AF_INET, "192.168.123.1", &dst_ip4);
	}

	{
		int ch;
		while ((ch = getopt(argc, argv, "d:D:f:l:m:s:S:t:")) != -1) {
			switch (ch) {
			case 'd':
				inet_pton(AF_INET, optarg, &dst_ip4);
				break;
			case 'D':
				assert(sscanf(optarg, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
							&mac_dst[0],
							&mac_dst[1],
							&mac_dst[2],
							&mac_dst[3],
							&mac_dst[4],
							&mac_dst[5]) == 6);
				break;
			case 'f':
				if (strlen(optarg) == 2 && !strncmp("tx", optarg, 2))
					mode_rx = 0;
				break;
			case 'l':
				assert(sscanf(optarg, "%hu", &pkt_len) == 1);
				break;
			case 'm':
				{
					{
						struct stat st;
						assert(!stat(optarg, &st));
						mem_size = st.st_size;
					}
					{
						int fd;
						assert((fd = open(optarg, O_RDWR)) != -1);
						assert((void *)(vif = (struct rvif *) mmap(NULL, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) != MAP_FAILED);
						printf("%s is mapped at %p (%lu bytes)\n", optarg, vif, mem_size);

					}
				}
				break;
			case 's':
				inet_pton(AF_INET, optarg, &src_ip4);
				break;
			case 'S':
				assert(sscanf(optarg, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
							&mac_src[0],
							&mac_src[1],
							&mac_src[2],
							&mac_src[3],
							&mac_src[4],
							&mac_src[5]) == 6);
				break;
			case 't':
				assert(sscanf(optarg, "%u", &num_thread) == 1);
				break;
			}
		}
	}

	assert(vif);

	vif->num = num_thread;

	{
		unsigned long mem_used = ((((sizeof(struct rvif) + sizeof(vif->queue[0]) * vif->num) / 0x1000) + 1) * 0x1000) + num_thread * 2 * num_slot * 2048;
		printf("rvif uses %lu bytes\n", mem_used);
		assert(mem_used < mem_size);
	}


	{
		char s[2][4 * 4];
		inet_ntop(AF_INET, &src_ip4, s[0], sizeof(s[0]));
		inet_ntop(AF_INET, &dst_ip4, s[1], sizeof(s[1]));
		printf("src %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx (%s) dst %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx (%s)\n",
				mac_src[0], mac_src[1], mac_src[2],
				mac_src[3], mac_src[4], mac_src[5], s[0],
				mac_dst[0], mac_dst[1], mac_dst[2],
				mac_dst[3], mac_dst[4], mac_dst[5], s[1]);
	}

	{
		unsigned short i;
		for (i = 0; i < vif->num; i++) {
			vif->queue[i].ring[0].num = vif->queue[i].ring[1].num = num_slot;
			vif->queue[i].ring[0].head = vif->queue[i].ring[1].head = 0;
			vif->queue[i].ring[0].tail = vif->queue[i].ring[1].tail = 0;
		}
	}

	{
		unsigned long off = (((sizeof(struct rvif) + sizeof(vif->queue[0]) * vif->num) / 0x1000) + 1) * 0x1000;
		{
			unsigned int i;
			for (i = 0; i < vif->num; i++) {
				{
					unsigned char j;
					for (j = 0; j < 2; j++) {
						unsigned short k;
						for (k = 0; k < vif->queue[i].ring[j].num; k++) {
							vif->queue[i].ring[j].slot[k].off = off;
							vif->queue[i].ring[j].slot[k].len = 0;
							off += 2048;
						}
					}
				}
				{
					char pkt[2048];
					memset(pkt, 'A', sizeof(pkt));
					{
						struct udpkt p = {
							.eth.src[0] = mac_src[0],
							.eth.src[1] = mac_src[1],
							.eth.src[2] = mac_src[2],
							.eth.src[3] = mac_src[3],
							.eth.src[4] = mac_src[4],
							.eth.src[5] = mac_src[5],
							.eth.dst[0] = mac_dst[0],
							.eth.dst[1] = mac_dst[1],
							.eth.dst[2] = mac_dst[2],
							.eth.dst[3] = mac_dst[3],
							.eth.dst[4] = mac_dst[4],
							.eth.dst[5] = mac_dst[5],
							.eth.type_be = htons(0x0800), /* ip */

							.ip4.l = 5, /* 5 * 4 = 20 bytes */
							.ip4.v = 4, /* ipv4 */
							.ip4.tos = 0,
							.ip4.len_be = htons(pkt_len - 14),
							.ip4.id_be = 0,
							.ip4.off_be = 0,
							.ip4.ttl = 64,
							.ip4.proto = 17,
							.ip4.src_be = src_ip4,
							.ip4.dst_be = dst_ip4,
							.ip4.csum_be = 0,

							.udp.src_be = htons(udp_src_port),
							.udp.dst_be = htons(udp_dst_port),
							.udp.len_be = htons(pkt_len - 34),
							.udp.csum_be = 0,
						};
						{
							struct bchain x = {
								.b = &((char *) &p)[14],
								.l = p.ip4.l * 4,
							};
							p.ip4.csum_be = htons(net_csum16(&x, 0));
						}
						{
							struct udp_pseudo pseudo = {
								.src = p.ip4.src_be,
								.dst = p.ip4.dst_be,
								.proto = p.ip4.proto,
								.len = p.udp.len_be,
							};
							struct bchain x[3] = {
								{
									.b = (char *) &pseudo,
									.l = sizeof(pseudo),
									.next = &x[1],
								},
								{
									.b = &((char *) &p)[34],
									.l = 8,
									.next = &x[2],
								},
								{
									.b = &pkt[42],
									.l = pkt_len - 42,
								}
							};
							p.udp.csum_be = htons(net_csum16(x, 0));
						}
						memcpy(pkt, &p, sizeof(p));
					}
					{
						unsigned short j;
						for (j = 0; j < vif->queue[i].ring[1].num; j++) {
							memcpy((char *)((unsigned long) vif + vif->queue[i].ring[1].slot[j].off), pkt, pkt_len);
							vif->queue[i].ring[1].slot[j].len = pkt_len;
						}
					}
				}
			}
		}
	}

	if (mode_rx)
		printf("-- RX --\n");
	else
		printf("-- TX --\n");

	{
		pthread_t *th;
		assert((th = calloc(vif->num, sizeof(pthread_t))) != NULL);
		{
			unsigned int i;
			for (i = 0; i < vif->num; i++)
				assert(!pthread_create(&th[i], NULL, pktgen_fn, (void *)((unsigned long) i)));
		}
		while (1) {
			sleep(1);
			global_counter_id = (global_counter_id ? 0 : 1);
			printf(" %4lu.%03lu Mpps ( %4lu.%03lu Gbps )\n",
					global_pkt_cnt[(global_counter_id ? 0 : 1)] / 1000000UL, (global_pkt_cnt[(global_counter_id ? 0 : 1)] % 1000000UL) / 1000UL,
					(8 * global_pkt_byte[(global_counter_id ? 0 : 1)]) / 1000000000UL, ((8 * global_pkt_byte[(global_counter_id ? 0 : 1)]) % 1000000000UL / 1000000UL));
			global_pkt_cnt[(global_counter_id ? 0 : 1)] = global_pkt_byte[(global_counter_id ? 0 : 1)] = 0;
		}
		{
			unsigned int i;
			for (i = 0; i < vif->num; i++)
				pthread_join(th[i], NULL);
		}
	}

	return 0;
}
