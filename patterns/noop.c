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
#include <unistd.h>

static uint64_t block_count() {
	return 0;
}

/* Pause the calling thread, preventing it from doing anything. */
static struct cmd next_cmd(struct ssd_features *ssd_features) {
	pause();
	return (struct cmd) {};
}

struct pattern pattern = {
	.desc = "Don't do anything.",
	.parse_arguments = NULL,
	.block_count = block_count,
	.next_cmd = next_cmd
};
