#pragma once

#include "hook_common.h"

void hookPrintIoctlDevFb(const char* prefix,
    const char* filename, int fd, request_t request, void* argp, int res);
