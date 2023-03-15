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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <pthread.h>

int rvs_lock_init(char *lock)
{
	return pthread_rwlock_init((pthread_rwlock_t *) lock, NULL);
}

int rvs_lock_destroy(char *lock)
{
	return pthread_rwlock_destroy((pthread_rwlock_t *) lock);
}

int rvs_wrlock(char *lock)
{
	return pthread_rwlock_wrlock((pthread_rwlock_t *) lock);
}

int rvs_wrunlock(char *lock)
{
	return pthread_rwlock_unlock((pthread_rwlock_t *) lock);
}

int rvs_rdlock(char *lock)
{
	return pthread_rwlock_rdlock((pthread_rwlock_t *) lock);
}

int rvs_rdunlock(char *lock)
{
	return pthread_rwlock_unlock((pthread_rwlock_t *) lock);
}

int rvs_notify(struct rvs *vs __attribute__((unused)),
	       unsigned short vid __attribute__((unused)),
	       unsigned short qid __attribute__((unused)))
{
	return 0;
}

static _Atomic unsigned long fwd_cnt = 0;

static void *monitor_th(void *data __attribute__((unused)))
{
	while (1) {
		sleep(1);
		{
			unsigned long cnt = fwd_cnt;
			fwd_cnt = 0;
			printf("%4lu.%06lu Mpps\n", cnt / 1000000UL, cnt % 1000000UL);
		}
	}
	pthread_exit(NULL);
}

int main(int argc, char *const *argv)
{
	unsigned short num_port = 0, batch_size = 512;
	struct rvs *vs;

	assert((vs = malloc(sizeof(struct rvs))) != NULL);

	assert(sizeof(pthread_rwlock_t) < RVS_LOCK_BUF_SIZE);

	assert(!rvs_init(vs));

	{
		int ch;
		while ((ch = getopt(argc, argv, "b:m:")) != -1) {
			switch (ch) {
				case 'b':
					assert(sscanf(optarg, "%hu", &batch_size));
					break;
				case 'm':
					{
						struct stat st;
						assert(!stat(optarg, &st));
						{
							int fd;
							assert((fd = open(optarg, O_RDWR)) != -1);
							{
								void *mem;
								assert((mem = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) != MAP_FAILED);
								printf("port[%u]: %s (%p)\n", num_port, optarg, mem);
								assert(!rvs_vif_attach(vs, num_port++, (struct rvif *) mem));
							}
						}
					}
					break;
			}
		}
	}

	{
		pthread_t th;

		assert(!pthread_create(&th, NULL, monitor_th, NULL));

		printf("-- FWD --\n");
		while (1) {
			unsigned short i;
			for (i = 0; i < num_port; i++) {
				unsigned short j;
				for (j = 0; j < vs->port[i].vif->num; j++)
					fwd_cnt += rvs_fwd(vs, i, j, batch_size);
			}
		}

		pthread_join(th, NULL);
	}

	assert(!rvs_exit(vs));

	free(vs);

	return 0;
}
