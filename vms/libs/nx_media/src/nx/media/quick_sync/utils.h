#pragma once

#include <QtMultimedia/QAbstractVideoBuffer>

#ifdef __linux__
    #include "linux/va_utils.h"
#endif
#ifdef _WIN32
    #include "windows/directx_utils.h"
#endif

constexpr int kHandleTypeQsvSurface = QAbstractVideoBuffer::UserHandle + 1;
