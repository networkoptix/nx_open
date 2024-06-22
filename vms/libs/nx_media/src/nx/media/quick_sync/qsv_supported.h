// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#if !defined(__arm__) && !defined(__aarch64__)
    #if defined(_WIN64)
        #define __QSV_SUPPORTED__
    #endif
#endif
