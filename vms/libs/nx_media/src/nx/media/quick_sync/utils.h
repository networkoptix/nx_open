// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#ifdef __linux__
    #include "linux/va_utils.h"
#endif
#ifdef _WIN32
    #include "windows/directx_utils.h"
#endif
