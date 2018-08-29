#ifdef _WIN32

#pragma once

#include "../device_data.h"

#include "camera/camera_plugin_types.h"

namespace nx {
namespace device {
namespace impl {

std::string getDeviceName(const char * devicePath);

std::vector<std::shared_ptr<AbstractCompressionTypeDescriptor>> getSupportedCodecs(const char * devicePath);

std::vector<DeviceData> getDeviceList();

std::vector<ResolutionData> getResolutionList(
    const char * devicePath,
    const std::shared_ptr<AbstractCompressionTypeDescriptor>& targetCodecID);

void setBitrate(const char * devicePath, int bitrate, nxcip::CompressionType targetCodecID);

int getMaxBitrate(const char * devicePath, nxcip::CompressionType targetCodecID);

} // namespace impl
} // namespace device
} // namespace nx

#endif
