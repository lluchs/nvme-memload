/*
 * Copyright © 2015 Lukas Werling
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
 */

#include <dlfcn.h>
#include <inttypes.h>
#include <linux/nvme.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "nvme.h"
#include "pattern.h"

#define DEVICE "/dev/nvme0n1"
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

static uint8_t *buffer;

static struct {
	int lba_shift;
	int max_block_count;
} ssd_features;

static void get_ssd_features() {
	int err;
	struct nvme_id_ns ns;
	struct nvme_id_ctrl ctrl;
	err = nvme_identify(&ns, 0);
	if (err < 0) return;
	err = nvme_identify(&ctrl, 1);

	ssd_features.lba_shift = ns.lbaf[ns.flbas].ds;
	ssd_features.max_block_count = pow(2, ctrl.mdts + 12 - ssd_features.lba_shift);
}

int main(int argc, char **argv) {
	int err = 0;
	nvme_open(DEVICE);
	get_ssd_features();

	printf("Block size: %i\n", 1 << ssd_features.lba_shift);
	printf("Max block count: %i\n", ssd_features.max_block_count);

	if (argc != 2) {
		fprintf(stderr, "Usage: %s pattern.so\n", argv[0]);
		exit(1);
	}

	// Get pattern to execute from the dynamic linker.
	void *handle = dlopen(argv[1], RTLD_LAZY);
	struct pattern *pattern = dlsym(handle, "pattern");
	char *error = dlerror();
	if (error != NULL) {
		fprintf(stderr, "%s\n", error);
		exit(1);
	}
	printf("Pattern loaded: %s\n%s\n\n", argv[1], pattern->desc);

	buffer = malloc(pattern->block_count() << ssd_features.lba_shift);

	struct timeval t;
	gettimeofday(&t, NULL);
	time_t sec = t.tv_sec;
	long block_count = 0;
	struct cmd cmd;
	for (;;) {
		cmd = pattern->next_cmd();
		switch (cmd.op) {
		case OP_WRITE:
		case OP_READ:
			for (int todo = cmd.block_count; todo > 0; todo -= ssd_features.max_block_count) {
				err = nvme_io(
						cmd.op,
						buffer + ((cmd.target_block + cmd.block_count - todo) << ssd_features.lba_shift),
						0,
						// XXX: Why is -1 necessary here?
						MIN(todo, ssd_features.max_block_count - 1));
				if (err != 0) exit(1);
			}
			block_count += cmd.block_count;
			break;
		default:
			fprintf(stderr, "Invalid command %d\n", cmd.op);
			exit(1);
		}
		gettimeofday(&t, NULL);
		if (sec != t.tv_sec) {
			printf("%ld blocks/s (%ld MiB/s)\n", block_count, (block_count << ssd_features.lba_shift) >> 20);
			block_count = 0;
			sec = t.tv_sec;
		}
	}

	dlclose(handle);
	return 0;
}
