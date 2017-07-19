#pragma once

#include <sys/types.h>

#include "hook_common.h"

void hookAfterOpen(const char* pathname, int flags, mode_t mode, int res);

void hookAfterClose(int fd, int res);

int hookHandleIoctl(int fd, request_t request, void* argp, IoctlFunc func_ioctl);
