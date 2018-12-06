#pragma once

#include <string>

#include <camera/camera_plugin.h>

namespace nx {
namespace usb_cam {
namespace device {
namespace audio {

void fillCameraAuxiliaryData(nxcip::CameraInfo* cameras, int cameraCount);
bool pluggedIn(const std::string& devicePath);

} // namespace audio
} // namespace device
} // namespace usb_cam
} // namespace nx