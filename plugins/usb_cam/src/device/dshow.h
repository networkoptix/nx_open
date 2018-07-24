#pragma once

#include "device_data.h"

namespace nx {
namespace device {
namespace impl

std::vector<nxcip::CompressionType> getSupportedCodecs(const char * devicePath);

std::vector<DeviceData> getDeviceList();

std::vector<ResolutionData> getResolutionList(
    const char * devicePath,
    nxcip::CompressionType codecID);

void setBitrate(const char * devicePath, int bitrate);

int getMaxBitrate(const char * devicePath);

} // namespace impl
} // namespace device
} // namespace nx
