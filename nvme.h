#include <linux/types.h>

void open_dev(const char *dev);

int identify(int namespace, void *ptr, int cns);
int read(void *buffer, __u64 start_block, __u16 block_count);
