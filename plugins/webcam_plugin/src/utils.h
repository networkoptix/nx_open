#pragma once
#include "device_info.h"
#ifdef _WIN32
#include "dshow_utils.h"
#elif __linux__
#elif __APPLE__
#endif

namespace nx {
namespace webcam_plugin {
namespace utils  {

QList<DeviceData> getDeviceList(bool getResolution = false);
QList<ResolutionData> getResolutionList(const char * devicePath);

} // namespace utils
} // namespace webcam_plugin
} // namespace nx