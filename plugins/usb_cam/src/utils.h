#pragma once

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace nx {
namespace usb_cam {
namespace utils {

std::string decodeCameraInfoUrl(const char * url);

std::string encodeCameraInfoUrl(const char * url);

} // namespace utils
} // namespace usb_cam
} // namespace nx