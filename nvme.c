// Adapted from nvme-cli: https://github.com/linux-nvme/nvme-cli
// Original license:
/*
 * nvme.c -- NVM-Express command line utility.
 *
 * Copyright (c) 2014-2015, Intel Corporation.
 *
 * Written by Keith Busch <keith.busch@intel.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nvme.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "linux/nvme.h"

static int fd;
static uint32_t nsid;

#define BATCH_COUNT 1000
static __thread struct nvme_batch_user_io *batch_io;

static const char *nvme_status_to_string(__u32 status)
{
	switch (status & 0x3ff) {
	case NVME_SC_SUCCESS:		return "SUCCESS";
	case NVME_SC_INVALID_OPCODE:	return "INVALID_OPCODE";
	case NVME_SC_INVALID_FIELD:	return "INVALID_FIELD";
	case NVME_SC_CMDID_CONFLICT:	return "CMDID_CONFLICT";
	case NVME_SC_DATA_XFER_ERROR:	return "DATA_XFER_ERROR";
	case NVME_SC_POWER_LOSS:	return "POWER_LOSS";
	case NVME_SC_INTERNAL:		return "INTERNAL";
	case NVME_SC_ABORT_REQ:		return "ABORT_REQ";
	case NVME_SC_ABORT_QUEUE:	return "ABORT_QUEUE";
	case NVME_SC_FUSED_FAIL:	return "FUSED_FAIL";
	case NVME_SC_FUSED_MISSING:	return "FUSED_MISSING";
	case NVME_SC_INVALID_NS:	return "INVALID_NS";
	case NVME_SC_CMD_SEQ_ERROR:	return "CMD_SEQ_ERROR";
	case NVME_SC_LBA_RANGE:		return "LBA_RANGE";
	case NVME_SC_CAP_EXCEEDED:	return "CAP_EXCEEDED";
	case NVME_SC_NS_NOT_READY:	return "NS_NOT_READY";
	case NVME_SC_CQ_INVALID:	return "CQ_INVALID";
	case NVME_SC_QID_INVALID:	return "QID_INVALID";
	case NVME_SC_QUEUE_SIZE:	return "QUEUE_SIZE";
	case NVME_SC_ABORT_LIMIT:	return "ABORT_LIMIT";
	case NVME_SC_ABORT_MISSING:	return "ABORT_MISSING";
	case NVME_SC_ASYNC_LIMIT:	return "ASYNC_LIMIT";
	case NVME_SC_FIRMWARE_SLOT:	return "FIRMWARE_SLOT";
	case NVME_SC_FIRMWARE_IMAGE:	return "FIRMWARE_IMAGE";
	case NVME_SC_INVALID_VECTOR:	return "INVALID_VECTOR";
	case NVME_SC_INVALID_LOG_PAGE:	return "INVALID_LOG_PAGE";
	case NVME_SC_INVALID_FORMAT:	return "INVALID_FORMAT";
	case NVME_SC_BAD_ATTRIBUTES:	return "BAD_ATTRIBUTES";
	case NVME_SC_WRITE_FAULT:	return "WRITE_FAULT";
	case NVME_SC_READ_ERROR:	return "READ_ERROR";
	case NVME_SC_GUARD_CHECK:	return "GUARD_CHECK";
	case NVME_SC_APPTAG_CHECK:	return "APPTAG_CHECK";
	case NVME_SC_REFTAG_CHECK:	return "REFTAG_CHECK";
	case NVME_SC_COMPARE_FAILED:	return "COMPARE_FAILED";
	case NVME_SC_ACCESS_DENIED:	return "ACCESS_DENIED";
	default:			return "Unknown";
	}
}

static void handle_nvme_error(const char *cmd, int err) {
	if (err < 0)
		perror("ioctl");
	else if (err)
		fprintf(stderr, "%s:%s(%04x)\n", cmd, nvme_status_to_string(err), err);
}

// Checks whether the NVMe driver supports our custom commands.
static bool nvme_has_custom_driver() {
	// Memoize the result.
	static int result = -1;
	return result != -1 ? result : (result = ioctl(fd, NVME_IOCTL_SUPPORTS_CUSTOM_CMDS) == 1);
}

// Needs to be called for every thread doing batch IO.
static void init_batch_io() {
	batch_io = malloc(sizeof(*batch_io) + BATCH_COUNT * sizeof(batch_io->cmds[0]));
	batch_io->count = BATCH_COUNT;
}


void nvme_open(const char *dev) {
	int err;
	fd = open(dev, O_RDONLY);
	if (fd < 0)
		goto perror;

	struct stat nvme_stat;
	err = fstat(fd, &nvme_stat);
	if (err < 0)
		goto perror;
	if (!S_ISCHR(nvme_stat.st_mode) && !S_ISBLK(nvme_stat.st_mode)) {
		fprintf(stderr, "%s is not a block or character device\n", dev);
		exit(ENODEV);
	}
	if (nvme_has_custom_driver()) {
		fprintf(stderr, "Custom driver commands are available.\n");
	}

	nsid = ioctl(fd, NVME_IOCTL_ID);
	return;
perror:
	perror(dev);
	exit(errno);
}


int nvme_identify(void *ptr, int cns) {
	struct nvme_admin_cmd cmd;
	int err;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = nvme_admin_identify;
	cmd.nsid = nsid;
	cmd.addr = (unsigned long)ptr;
	cmd.data_len = 4096;
	cmd.cdw10 = cns;
	err = ioctl(fd, NVME_IOCTL_ADMIN_CMD, &cmd);
	handle_nvme_error("identify", err);
	return err;
}

int nvme_io(int op, void *buffer, __u64 start_block, __u16 block_count) {
	static __thread int i = 0;

	struct nvme_user_io io;
	int err = 0;

	memset(&io, 0, sizeof(io));

	io.opcode  = op;
	io.slba    = start_block;
	io.nblocks = block_count;
	io.addr    = (__u64)buffer;

	if (nvme_has_custom_driver()) {
		if (!batch_io) init_batch_io();
		// With the custom driver, we buffer commands for submission to save on
		// syscalls.
		batch_io->cmds[i++] = io;
		if (i == BATCH_COUNT) {
			err = ioctl(fd, NVME_IOCTL_SUBMIT_BATCH_IO, batch_io);
			handle_nvme_error("batched read/write", err);
			i = 0;
		}
	} else {
		err = ioctl(fd, NVME_IOCTL_SUBMIT_IO, &io);
		handle_nvme_error("read/write", err);
	}
	return err;
}

int nvme_io_cmd(int op) {
	struct nvme_passthru_cmd cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = op;
	cmd.nsid = nsid;
	int err = ioctl(fd, NVME_IOCTL_IO_CMD, &cmd);
	handle_nvme_error("io cmd", err);
	return err;
}
