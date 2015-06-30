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

#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "pattern.h"

static uint64_t block_count = 1000;
static int operation = OP_WRITE;

// Parses command-line arguments.
void parse_options(int argc, char **argv) {
	int opt;
	optind = 1;
	while ((opt = getopt(argc, argv, "hb:o:")) != -1) {
		switch (opt) {
		case 'b':
			block_count = atoll(optarg);
			break;
		case 'o':
			if (!strcmp("read", optarg))       operation = OP_READ;
			else if (!strcmp("write", optarg)) operation = OP_WRITE;
			else {
				fprintf(stderr, "Invalid option -o %s\n", optarg);
				exit(1);
			}
			break;
		case 'h':
		default:
			fprintf(stderr, "Usage: %s -b <buffer size> -o <read/write>\n", argv[0]);
			exit(1);
		}
	}
}

uint64_t opt_block_count() { return block_count; }
int opt_operation() { return operation; }
