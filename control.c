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

#include "control.h"

int block_count() {
	return 100;
}

/* Always write to the full buffer. */
struct cmd next_cmd() {
	return (struct cmd) {
		.op = OP_WRITE,
		.block_count = block_count(),
		.target_block = 0
	};
}
// */

/* Sequentially write a single block to memory. * /
struct cmd next_cmd() {
	static int current = 0;
	current %= block_count();
	return (struct cmd) {
		.op = OP_WRITE,
		.block_count = 1,
		.target_block = current++
	};
}
// */
