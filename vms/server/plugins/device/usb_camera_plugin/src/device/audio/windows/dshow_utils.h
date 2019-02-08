#ifdef _WIN32

#pragma once

#include <string>

#include <camera/camera_plugin.h>

namespace nx {
namespace usb_cam {
namespace device {
namespace audio {
namespace detail {

void fillCameraAuxiliaryData(nxcip::CameraInfo * cameras, int cameraCount);
bool pluggedIn(const std::string& devicePath);

} // namespace detail
} // namespace audio
} // namespace device
} // namespace usb_cam
} // namespace nx

#endif

