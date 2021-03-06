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
#include "common/options.h"

/* Sequentially write a single block to memory. */
struct cmd next_cmd() {
	static int current = 0;
	current %= opt_block_count() - 1;
	return (struct cmd) {
		.op = opt_operation(),
		.block_count = 1,
		.target_block = current++
	};
}

struct pattern pattern = {
	.desc = "Sequentially access a single block.",
	.parse_arguments = parse_options,
	.block_count = opt_block_count,
	.next_cmd = next_cmd
};
