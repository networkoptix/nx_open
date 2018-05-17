#pragma once
#include "device_data.h"

namespace nx {
namespace webcam_plugin {
namespace utils  {

QList<DeviceData> getDeviceList(bool getResolution = false);
QList<ResolutionData> getResolutionList(const char * devicePath);

} // namespace utils
} // namespace webcam_plugin
} // namespace nx