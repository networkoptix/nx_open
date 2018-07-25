#pragma once

#include "device_data.h"

#include "camera/camera_plugin_types.h"

namespace nx {
namespace device {
namespace impl {

std::string getDeviceName(const char * devicePath);

std::vector<nxcip::CompressionType> getSupportedCodecs(const char * devicePath);

std::vector<DeviceData> getDeviceList();

std::vector<ResolutionData> getResolutionList(
    const char * devicePath,
    nxcip::CompressionType targetCodecID);

void setBitrate(const char * devicePath, int bitrate);

int getMaxBitrate(const char * devicePath, nxcip::CompressionType targetCodecID);

} // namespace impl
} // namespace device
} // namespace nx
