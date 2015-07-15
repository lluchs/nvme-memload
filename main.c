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
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>
#include "linux/nvme.h" // Local header with additions.
#include "nvme.h"
#include "pattern.h"
#include "random.h"

static uint8_t *buffer;
static struct ssd_features ssd_features;
static pthread_mutex_t pattern_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct pattern *pattern;
static long long block_limit, command_limit;
static pthread_mutex_t limit_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t limit_cond = PTHREAD_COND_INITIALIZER;

static struct {
	bool cache_once;
	bool cache_always;
	int parallelism;
	long long block_limit;
	long long command_limit;
} opts = {
	.cache_once = false,
	.cache_always = false,
	.parallelism = 1,
	.block_limit = 0,
	.command_limit = 0,
};

struct worker_state {
	pthread_t thread_id;
	uint64_t block_count;
	uint64_t command_count;
};

// Used to move values to the cache, must not be optimized out.
uint8_t dummy_sum;

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
	if (cmd->op == OP_FLUSH) {
		err = nvme_io_cmd(cmd->op);
	} else {
		err = nvme_io(
				cmd->op,
				buffer + (cmd->target_block << ssd_features.lba_shift),
				ssd_block,
				cmd->block_count);
	}
	if (err != 0) exit(1);
}

static void put_in_cache(size_t start, size_t count) {
	for (size_t i = 0; i < count; i++) {
		dummy_sum += buffer[start + i];
	}
}

static void init_worker(struct worker_state *state) {
	state->block_count = 0;
	state->command_count = 0;
}

static void *run_worker(void *arg) {
	struct worker_state *state = arg;
	struct cmd cmd;
	for (;;) {
		// Get a new command. The patterns usually have internal state, so we need a mutex.
		pthread_mutex_lock(&pattern_mutex);
		cmd = pattern->next_cmd(&ssd_features);
		pthread_mutex_unlock(&pattern_mutex);

		if (opts.block_limit > 0 || opts.command_limit > 0) {
			// The limit is shared by all workers and periodically reset by the main thread.
			pthread_mutex_lock(&limit_mutex);
			while ((opts.block_limit   > 0 && block_limit   <= 0)
			    || (opts.command_limit > 0 && command_limit <= 0))
				pthread_cond_wait(&limit_cond, &limit_mutex);
			// Allow a single operation to go over the limit.
			block_limit -= cmd.block_count;
			command_limit -= 1;
			pthread_mutex_unlock(&limit_mutex);
		}

		if (opts.cache_always)
			put_in_cache(cmd.target_block << ssd_features.lba_shift, cmd.block_count << ssd_features.lba_shift);

		perform_io(&cmd);

		state->block_count += cmd.block_count;
		state->command_count++;
	}
	return NULL;
}

static void usage(char *name) {
	fprintf(stderr, "Usage: %s [options] /dev/nvme0n1 pattern [pattern options]\n", name);
	fprintf(stderr, "\nOptions:\n");
	fprintf(stderr, "\t-c mode\tMake sure blocks are cached <once/always> before reading/writing.\n");
	fprintf(stderr, "\t-j num\tSend commands in parallel on <num> threads.\n");
	fprintf(stderr, "\t-l num\tLimit transfers to <num> blocks/s.\n");
	fprintf(stderr, "\t-L num\tLimit transfers to <num> commands/s.\n");
	exit(1);
}

int main(int argc, char **argv) {
	if (argc < 3) usage(argv[0]);

	// stdout is block buffered when writing to a file which may prevent output
	// from showing up in the benchmark log. Always using line buffering fixes this.
	setlinebuf(stdout);

	// Options
	int opt; // +: Stop parsing arguments when the first non-option is encountered.
	while ((opt = getopt(argc, argv, "+c:j:l:L:h")) != -1) {
		switch (opt) {
		case 'c':
			if (!strcmp(optarg, "once"))
				opts.cache_once = true;
			else if (!strcmp(optarg, "always"))
				opts.cache_always = true;
			else
				usage(argv[0]);
			break;
		case 'j':
			opts.parallelism = atoi(optarg);
			break;
		case 'l':
			opts.block_limit = atoll(optarg);
			break;
		case 'L':
			opts.command_limit = atoll(optarg);
			break;
		case 'h':
		default:
			usage(argv[0]);
		}
	}

	init_random();
	nvme_open(argv[optind]);
	get_ssd_features();

	printf("SSD: %s (%s)\n", ssd_features.mn, ssd_features.sn);
	printf("SSD size: %"PRIu64" blocks (%"PRIu64" GiB)\n", ssd_features.size, (ssd_features.size << ssd_features.lba_shift) >> 30);
	printf("Block size: %i B\n", 1 << ssd_features.lba_shift);
	printf("Max block count: %i blocks per command\n", ssd_features.max_block_count);

	// Get pattern to execute from the dynamic linker.
	char *pattern_path = get_pattern_path(argv[optind + 1]);
	printf("Loading pattern %s\n", pattern_path);
	void *handle = dlopen(pattern_path, RTLD_LAZY);
	pattern = dlsym(handle, "pattern");
	char *error = dlerror();
	if (error != NULL) {
		fprintf(stderr, "%s\n", error);
		exit(1);
	}
	if (pattern->parse_arguments != NULL) pattern->parse_arguments(argc - optind - 1, argv + optind + 1);
	printf("Memory buffer size: %"PRIu64" blocks (%"PRIu64" MiB)\n", pattern->block_count(), (pattern->block_count() << ssd_features.lba_shift) >> 20);
	printf("Pattern loaded: %s\n\n", pattern->desc);

	buffer = malloc(pattern->block_count() << ssd_features.lba_shift);
	if (buffer == NULL) {
		perror("malloc");
		exit(1);
	}
	if (opts.cache_once)
		put_in_cache(0, pattern->block_count() << ssd_features.lba_shift);

	block_limit = opts.block_limit;
	command_limit = opts.command_limit;

	struct worker_state workers[opts.parallelism];
	for (int i = 0; i < opts.parallelism; i++) {
		init_worker(&workers[i]);
		pthread_create(&workers[i].thread_id, NULL, run_worker, &workers[i]);
	}

	struct timespec t = { .tv_sec = 1, .tv_nsec = 0 };
	uint64_t block_count, command_count;
	for (;;) {
		nanosleep(&t, NULL);

		block_count = 0; command_count = 0;
		for (int i = 0; i < opts.parallelism; i++) {
			// XXX: Race condition
			block_count += workers[i].block_count;
			command_count += workers[i].command_count;
			workers[i].block_count = 0;
			workers[i].command_count = 0;
		}
		printf("%"PRIu64" blocks/s (%"PRIu64" MiB/s)", block_count, (block_count << ssd_features.lba_shift) >> 20);
		// Show command number and size when it's large enough to matter.
		uint64_t command_size = (command_count * (sizeof(struct nvme_rw_command) + sizeof(struct nvme_completion))) >> 20;
		if (command_size)
			printf(" via %"PRIu64" commands (%"PRIu64" MiB/s)\n", command_count, command_size);
		else
			putchar('\n');

		if (opts.block_limit > 0 || opts.command_limit > 0) {
			// Reset the limit and notify all workers.
			pthread_mutex_lock(&limit_mutex);
			block_limit = opts.block_limit;
			command_limit = opts.command_limit;
			pthread_cond_broadcast(&limit_cond);
			pthread_mutex_unlock(&limit_mutex);
		}
	}

	dlclose(handle);
	return 0;
}
