#pragma once

#include <string>

#include <camera/camera_plugin.h>

namespace nx::usb_cam::device::audio {

void fillCameraAuxiliaryData(nxcip::CameraInfo* cameras, int cameraCount);
bool pluggedIn(const std::string& devicePath);

} // namespace nx::usb_cam::device::audio
