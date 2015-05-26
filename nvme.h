#include <linux/types.h>

void open_nvme(const char *dev);

int identify(void *ptr, int cns);
int read_nvme(void *buffer, __u64 start_block, __u16 block_count);
