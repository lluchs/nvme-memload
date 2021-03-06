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
#include "random.h"
#include "common/options.h"

/* Always write to the full buffer. */
static struct cmd next_cmd(struct ssd_features *ssd_features) {
	return (struct cmd) {
		.op = opt_operation(),
		.block_count = ssd_features->max_block_count - 1,
		.target_block = get_random_block(opt_block_count(), ssd_features->max_block_count - 1)
	};
}

struct pattern pattern = {
	.desc = "Accesses random disk blocks in large chunks.",
	.parse_arguments = parse_options,
	.block_count = opt_block_count,
	.next_cmd = next_cmd
};
