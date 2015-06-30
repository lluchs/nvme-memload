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
#include <libgen.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "linux/nvme.h" // Local header with additions.
#include "nvme.h"
#include "pattern.h"
#include "random.h"

static uint8_t *buffer;
static struct ssd_features ssd_features;

static char *get_pattern_path(char *pattern) {
	static char buffer[255];
	char *ext = strrchr(pattern, '.');
	// Ends with .so => assume it's a path.
	if (ext && !strcmp(ext, ".so")) return pattern;
	// Construct path relative to executable.
	if (readlink("/proc/self/exe", buffer, sizeof(buffer)) < 0) {
		fprintf(stderr, "Could not resolve pattern path.\n");
		return pattern;
	}
	char *dir = dirname(buffer);
	// dirname will most likely modify buffer.
	memmove(buffer, dir, sizeof(buffer));
	size_t len = strlen(buffer);
	snprintf(buffer + len, sizeof(buffer) - len, "/patterns/%s.so", pattern);
	return buffer;
}

static void get_ssd_features() {
	int err;
	struct nvme_id_ns ns;
	struct nvme_id_ctrl ctrl;
	err = nvme_identify(&ns, 0);
	if (err < 0) return;
	err = nvme_identify(&ctrl, 1);

	memcpy(ssd_features.sn, ctrl.sn, 20);
	ssd_features.sn[20] = 0;
	memcpy(ssd_features.mn, ctrl.mn, 40);
	ssd_features.mn[40] = 0;
	ssd_features.size = ns.nsze;
	ssd_features.lba_shift = ns.lbaf[ns.flbas].ds;
	ssd_features.max_block_count = pow(2, ctrl.mdts + 12 - ssd_features.lba_shift);
}

// Performs an IO (i.e. read/write to SSD) command.
static void perform_io(struct cmd *cmd) {
	int err;
	uint64_t ssd_block = 0;
	// Randomize SSD write target for optimal performance.
	if (cmd->op == OP_READ) ssd_block = get_random_block(ssd_features.size, cmd->block_count);
	err = nvme_io(
			cmd->op,
			buffer + (cmd->target_block << ssd_features.lba_shift),
			ssd_block,
			cmd->block_count);
	if (err != 0) exit(1);
}

int main(int argc, char **argv) {
	if (argc < 3) {
		fprintf(stderr, "Usage: %s /dev/nvme0n1 pattern.so [options]\n", argv[0]);
		exit(1);
	}

	// stdout is block buffered when writing to a file which may prevent output
	// from showing up in the benchmark log. Always using line buffering fixes this.
	setlinebuf(stdout);

	init_random();
	nvme_open(argv[1]);
	get_ssd_features();

	printf("SSD: %s (%s)\n", ssd_features.mn, ssd_features.sn);
	printf("SSD size: %"PRIu64" blocks (%"PRIu64" GiB)\n", ssd_features.size, (ssd_features.size << ssd_features.lba_shift) >> 30);
	printf("Block size: %i B\n", 1 << ssd_features.lba_shift);
	printf("Max block count: %i blocks per command\n", ssd_features.max_block_count);

	// Get pattern to execute from the dynamic linker.
	char *pattern_path = get_pattern_path(argv[2]);
	printf("Loading pattern %s\n", pattern_path);
	void *handle = dlopen(pattern_path, RTLD_LAZY);
	struct pattern *pattern = dlsym(handle, "pattern");
	char *error = dlerror();
	if (error != NULL) {
		fprintf(stderr, "%s\n", error);
		exit(1);
	}
	if (pattern->parse_arguments != NULL) pattern->parse_arguments(argc - 2, argv + 2);
	printf("Memory buffer size: %"PRIu64" blocks (%"PRIu64" MiB)\n", pattern->block_count(), (pattern->block_count() << ssd_features.lba_shift) >> 20);
	printf("Pattern loaded: %s\n%s\n\n", argv[2], pattern->desc);

	buffer = malloc(pattern->block_count() << ssd_features.lba_shift);
	if (buffer == NULL) {
		perror("malloc");
		exit(1);
	}

	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC_COARSE, &t);
	time_t sec = t.tv_sec;
	uint64_t block_count = 0, command_count = 0;
	struct cmd cmd;
	for (;;) {
		cmd = pattern->next_cmd(&ssd_features);
		switch (cmd.op) {
		case OP_WRITE:
		case OP_READ:
			perform_io(&cmd);
			block_count += cmd.block_count;
			command_count++;
			break;
		default:
			fprintf(stderr, "Invalid command %d\n", cmd.op);
			exit(1);
		}
		clock_gettime(CLOCK_MONOTONIC_COARSE, &t);
		if (sec != t.tv_sec) {
			printf("%"PRIu64" blocks/s (%"PRIu64" MiB/s)", block_count, (block_count << ssd_features.lba_shift) >> 20);
			// Show command number and size when it's large enough to matter.
			uint64_t command_size = (command_count * (sizeof(struct nvme_rw_command) + sizeof(struct nvme_completion))) >> 20;
			if (command_size)
				printf(" via %"PRIu64" commands (%"PRIu64" MiB/s)\n", command_count, command_size);
			else
				putchar('\n');
			block_count = 0;
			command_count = 0;
			sec = t.tv_sec;
		}
	}

	dlclose(handle);
	return 0;
}
