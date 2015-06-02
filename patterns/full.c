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

#include "pattern.h"

static uint64_t block_count() {
	return 1000000;
}

/* Always write to the full buffer. */
static struct cmd next_cmd(struct ssd_features *ssd_features) {
	static uint64_t todo = 0;
	uint64_t count = block_count();
	if (todo == 0) todo = count;

	size_t target_block = count - todo;
	if (todo > ssd_features->max_block_count)
		todo -= ssd_features->max_block_count;
	else
		todo = 0;

	return (struct cmd) {
		.op = OP_WRITE,
		// XXX: Why is -1 necessary here?
		.block_count = MIN(todo, ssd_features->max_block_count - 1),
		.target_block = target_block
	};
}

struct pattern pattern = {
	.desc = "Sequentially write as much as possible at once.",
	.block_count = block_count,
	.next_cmd = next_cmd
};
