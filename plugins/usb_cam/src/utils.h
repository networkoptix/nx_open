#pragma once

#include <string>
#include <chrono>

namespace nx {
namespace usb_cam {
namespace utils {

std::string decodeCameraInfoUrl(const char * url);

std::string encodeCameraInfoUrl(const char * host, const char * cameraResource);

} // namespace utils
} // namespace usb_cam
} // namespace nx