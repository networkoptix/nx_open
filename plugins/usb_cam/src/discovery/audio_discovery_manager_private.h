#pragma once

#include <camera/camera_plugin.h>

namespace nx {
namespace usb_cam {
namespace device {

class AudioDiscoveryManagerPrivate
{
public:
    virtual void fillCameraAuxData(nxcip::CameraInfo* cameras, int cameraCount) const = 0;
};

} // namespace device
} // namespace usb_cam
} // namespace nx