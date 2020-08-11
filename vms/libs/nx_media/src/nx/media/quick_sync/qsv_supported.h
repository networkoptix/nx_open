#pragma once

#if !defined(__arm__) && !defined(__aarch64__)
    #if defined(_WIN64) || defined(__linux__)
        #define __QSV_SUPPORTED__
    #endif
#endif

