#include "nvme.h"
#include <linux/nvme.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define DEVICE "/dev/nvme0n1"
#define BLOCK_COUNT 50

/* static void *buffer; */

static struct {
	int lba_shift;
	int max_block_count;
} ssd_features;

static void get_ssd_features() {
	int err;
	struct nvme_id_ns ns;
	struct nvme_id_ctrl ctrl;
	err = identify(&ns, 0);
	if (err < 0) return;
	err = identify(&ctrl, 1);

	ssd_features.lba_shift = ns.lbaf[ns.flbas].ds;
	ssd_features.max_block_count = pow(2, ctrl.mdts + 12 - ssd_features.lba_shift);
}

int main(int argc, char **argv) {
	/* int err = 0; */
	open_nvme(DEVICE);
	get_ssd_features();

	printf("Block size: %i\n", 1 << ssd_features.lba_shift);
	printf("Max block count: %i\n", ssd_features.max_block_count);

	/* buffer = malloc(BLOCK_COUNT); */

	return 0;
}
