#pragma once

#include <QtCore/QtGlobal>

#if defined(__APPLE__) || defined(Q_OS_ANDROID) || defined(Q_OS_IOS) || defined(__aarch64__)
    #include <zlib.h>
#else
    #include <QtZlib/zlib.h>
#endif
