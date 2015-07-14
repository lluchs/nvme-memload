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
	return 0;
}

/* Just send flush commands. */
static struct cmd next_cmd(struct ssd_features *ssd_features) {
	return (struct cmd) {
		.op = OP_FLUSH,
		.block_count = 0,
		.target_block = 0
	};
}

struct pattern pattern = {
	.desc = "Send only flush commands, no data transfer.",
	.parse_arguments = NULL,
	.block_count = block_count,
	.next_cmd = next_cmd
};
