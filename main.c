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
#include <fcntl.h>
#include <inttypes.h>
#include <linux/nvme.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include "nvme.h"
#include "pattern.h"

#define DEVICE "/dev/nvme0n1"

static uint8_t *buffer;
static struct ssd_features ssd_features;

static void get_ssd_features() {
	int err;
	struct nvme_id_ns ns;
	struct nvme_id_ctrl ctrl;
	err = nvme_identify(&ns, 0);
	if (err < 0) return;
	err = nvme_identify(&ctrl, 1);

	ssd_features.size = ns.nsze;
	ssd_features.lba_shift = ns.lbaf[ns.flbas].ds;
	ssd_features.max_block_count = pow(2, ctrl.mdts + 12 - ssd_features.lba_shift);
}

// Initializes the random number generator.
static void init_random() {
	// Get a truly random seed so that even two instances started at the same
	// time will not use the same sequence.
	int fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0) goto perror;
	unsigned int seed;
	if (read(fd, &seed, sizeof(seed)) != sizeof(seed)) goto perror;
	srand(seed);
	close(fd);
	return;
perror:
	perror("init_random");
	exit(1);
}

// Returns a random block number which allows accessing `size` blocks.
static uint64_t get_random_block(uint16_t size) {
	uint64_t max = ssd_features.size;
	// This isn't uniform, but hopefully random enough. It also won't use all
	// of very large SSDs.
	return rand() % (max - size);
}

// Performs an IO (i.e. read/write to SSD) command.
static void perform_io(struct cmd *cmd) {
	int err;
	uint64_t ssd_block = 0;
	// Randomize SSD write target for optimal performance.
	if (cmd->op == OP_READ) ssd_block = get_random_block(cmd->block_count);
	err = nvme_io(
			cmd->op,
			buffer + (cmd->target_block << ssd_features.lba_shift),
			ssd_block,
			cmd->block_count);
	if (err != 0) exit(1);
}

int main(int argc, char **argv) {
	init_random();
	nvme_open(DEVICE);
	get_ssd_features();

	printf("SSD size: %"PRIu64" blocks (%"PRIu64" GiB)\n", ssd_features.size, (ssd_features.size << ssd_features.lba_shift) >> 30);
	printf("Block size: %i B\n", 1 << ssd_features.lba_shift);
	printf("Max block count: %i blocks per command\n", ssd_features.max_block_count);

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
	if (buffer == NULL) {
		perror("malloc");
		exit(1);
	}

	struct timeval t;
	gettimeofday(&t, NULL);
	time_t sec = t.tv_sec;
	uint64_t block_count = 0;
	struct cmd cmd;
	for (;;) {
		cmd = pattern->next_cmd(&ssd_features);
		switch (cmd.op) {
		case OP_WRITE:
		case OP_READ:
			perform_io(&cmd);
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
