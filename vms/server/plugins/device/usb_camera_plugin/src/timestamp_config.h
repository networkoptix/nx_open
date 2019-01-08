#pragma once

extern "C"{
#include <libavutil/time.h>
}

namespace nx {
namespace usb_cam{

#define USE_MSEC

#if defined(USE_MSEC)
typedef std::chrono::milliseconds duration_t;
#else
typedef std::chrono::microseconds duration_t;
#endif

inline uint64_t usbGetTime()
{
#if defined(USE_MSEC)
    return av_gettime() / 1000;
#else
    return av_gettime();
#endif
}

}
}

