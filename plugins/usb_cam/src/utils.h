#pragma once

#include <memory>

#include "device/device_data.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace nx {
namespace usb_cam {
namespace utils {

std::string decodeCameraInfoUrl(const char * url);

std::string encodeCameraInfoUrl( const char * url);

std::vector<AVCodecID> ffmpegCodecPriorityList();

std::shared_ptr<device::AbstractCompressionTypeDescriptor> getPriorityDescriptor(
    const std::vector<std::shared_ptr<device::AbstractCompressionTypeDescriptor>>& codecDescriptorList);

} // namespace utils
} // namespace usb_cam
} // namespace nx