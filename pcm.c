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

#include "pcm.h"

#include "intelpcm/intelpcm.h"

#include <dlfcn.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define PCM_OPERATIONS \
	OP(PCIeRdCur) \
	OP(PCIeNSRd) \
	OP(PCIeWiLF) \
	OP(PCIeItoM) \
	OP(PCIeNSWr) \
	OP(PCIeNSWrF) \
	OP(RFO) \
	OP(CRd) \
	OP(DRd) \
	OP(PRd) \
	OP(WCiLF) \
	OP(WCiL) \
	OP(WiL) \
	OP(WbMtoI) \
	OP(WbMtoE) \
	OP(ItoM) \
	OP(WB) \
	OP(AnyOp)


#define OP(op) #op,
static const char* pcm_operation_list[] = {
	PCM_OPERATIONS
	NULL
};
#undef OP

static pcm_handle_t instance;
static enum CBoxOpc opcode;
static struct timespec start_time;
static enum tracking_mode {
	hits = 0,
	misses = 1,
} tracking_mode;

static enum CBoxOpc str_to_opcode(const char *str) {
#define OP(op) if (!strncmp(str, #op, sizeof(#op))) return op;
	PCM_OPERATIONS
#undef OP
	return -1;
}

static const char * opcode_to_str(enum CBoxOpc opcode) {
	switch (opcode) {
#define OP(op) case op: return #op;
	PCM_OPERATIONS
#undef OP
	default:
		return "unknown operation";
	}
}

static enum tracking_mode str_to_tracking_mode(const char *str) {
	if (str != NULL) {
		if (strcmp(str, "hits") == 0) return hits;
		if (strcmp(str, "misses") == 0) return misses;
	}
	return -1;
}

static void exit_handler() {
	printf("\n\n");

	struct timespec end_time;
	clock_gettime(CLOCK_MONOTONIC, &end_time);
	double diff = end_time.tv_sec - start_time.tv_sec + (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

	uint64_t counter = pcm_get_value();
	long double per_second = (long double)counter / diff;
	printf("%s: %"PRIu64" over %.2fs (%.0Lf per second)\n", pcm_get_counter_name(), counter, diff, per_second);
}

void pcm_parse_optarg(const char *_optarg) {
	char * optarg = strdup(_optarg);
	char * opcode_str = strtok(optarg, "-");
	char * tracking_mode_str = strtok(NULL, "");
	opcode = str_to_opcode(opcode_str);
	tracking_mode = str_to_tracking_mode(tracking_mode_str);
	if (opcode == -1 || tracking_mode == -1) {
		fprintf(stderr, "Error: Invalid option -p %s\n", _optarg);
		fprintf(stderr, "Valid opcodes are: ");
		for (const char **p = pcm_operation_list; *p; p++)
			fprintf(stderr, "%s ", *p);
		fprintf(stderr, "\n");
		fprintf(stderr, "Valid suffixes are -hits or -misses.\n");
		exit(1);
	}

	void *lib = dlopen("libintelpcm.so", RTLD_NOW | RTLD_GLOBAL);
	if (lib) {
		instance = getInstance();
	} else {
		fprintf(stderr, "Error: Intel PCM not available. %s\n", dlerror());
		exit(1);
	}

	free(optarg);
}

void pcm_enable() {
	// Print statistics on exit.
	atexit(exit_handler);

	programPCIeCounters(instance, opcode, 0, tracking_mode);
	clock_gettime(CLOCK_MONOTONIC, &start_time);
}

const char * pcm_get_counter_name() {
	return opcode_to_str(opcode);
}

uint64_t pcm_get_value() {
	return getPCIeCounters(instance, 0);
}
