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

enum {
	OP_WRITE, // write to memory
};

struct cmd {
	// Operation
	int op;
	// How many blocks to read.
	int block_count;
	// The target position in memory.
	int target_block;
};

struct pattern {
	const char *desc;

	// Returns the size of the memory buffer in blocks.
	int (*block_count)();

	// Returns the next thing to do.
	struct cmd (*next_cmd)();
};
