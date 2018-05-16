#pragma once
#include "device_info.h"
#ifdef _WIN32
#include "dshow_utils.h"
#elif __linux__
#elif __APPLE__
#endif

namespace utils 
{
    QList<DeviceInfo> getDeviceList(bool getResolution = false);
    QList<QSize> getResolutionList(const char * devicePath);
}