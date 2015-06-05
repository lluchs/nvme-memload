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

#include <inttypes.h>
#include <stdlib.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

enum {
	// These correspond to the NVMe opcodes. Note that NVMe commands are from
	// SSD perspective.
	OP_READ  = 1, // read from memory
	OP_WRITE = 2, // write to memory
};

struct ssd_features {
	char sn[21];
	char mn[41];
	uint64_t size;
	int lba_shift;
	int max_block_count;
};

struct cmd {
	// Operation
	int op;
	// How many blocks to read.
	uint16_t block_count;
	// The target position in memory.
	size_t target_block;
};

struct pattern {
	const char *desc;

	// Function to parse command line arguments.
	void (*parse_arguments)(int, char**);

	// Returns the size of the memory buffer in blocks.
	uint64_t (*block_count)();

	// Returns the next thing to do.
	struct cmd (*next_cmd)(struct ssd_features*);
};
