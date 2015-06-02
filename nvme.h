#include <linux/types.h>

void nvme_open(const char *dev);

int nvme_identify(void *ptr, int cns);
int nvme_read(void *buffer, __u64 start_block, __u16 block_count);
