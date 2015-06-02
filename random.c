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

#include "random.h"
#include "pattern.h"
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

// Initializes the random number generator.
void init_random() {
	// Get a truly random seed so that even two instances started at the same
	// time will not use the same sequence.
	int fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0) goto perror;
	unsigned int seed;
	if (read(fd, &seed, sizeof(seed)) != sizeof(seed)) goto perror;
	srand(seed);
	close(fd);
	return;
perror:
	perror("init_random");
	exit(1);
}

// Returns a random block number which allows accessing `size` blocks.
uint64_t get_random_block(uint64_t max, uint16_t size) {
	// This isn't uniform, but hopefully random enough. It also won't use all
	// of very large SSDs.
	return rand() % (max - size);
}
