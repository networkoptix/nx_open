#pragma once

#include "device_data.h"

namespace nx {
namespace webcam_plugin {
namespace utils {
namespace dshow {

std::vector<nxcip::CompressionType> getSupportedCodecs(const char * devicePath);

std::vector<DeviceData> getDeviceList(
    bool getResolution,
    nxcip::CompressionType codecID);

std::vector<ResolutionData> getResolutionList(
    const char * devicePath,
    nxcip::CompressionType codecID);

} // namespace dshow
} // namespace utils
} // namespace webcam_plugin
} // namespace nx
