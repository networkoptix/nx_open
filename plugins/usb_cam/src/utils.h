#pragma once

#include <string>

namespace nx {
namespace usb_cam {
namespace utils {

std::string decodeCameraInfoUrl(const char * url);

std::string encodeCameraInfoUrl(const char * url);

float msecPerFrame(float fps);

} // namespace utils
} // namespace usb_cam
} // namespace nx