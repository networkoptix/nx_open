#ifdef __linux__

#pragma once

#include <string>

namespace nx {
namespace usb_cam {
namespace device {
namespace video {
namespace detail {

std::string getDeviceUniqueId(const std::string& devicePath);

} // namespace detail
} // namespace video
} // namespace device
} // namespace usb_cam
} // namespace nx

#endif // __linux__