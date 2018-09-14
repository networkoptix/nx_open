#pragma once

#include <camera/camera_plugin.h>

namespace nx {
namespace usb_cam {
namespace device {

class AudioDiscoveryManagerPrivate
{
public:
    virtual ~AudioDiscoveryManagerPrivate();
    virtual void fillCameraAuxData(nxcip::CameraInfo* cameras, int cameraCount) const = 0;
};

class AudioDiscoveryManager
{
public:
    AudioDiscoveryManager();
    ~AudioDiscoveryManager();
    void fillCameraAuxData(nxcip::CameraInfo* cameras, int cameraCount) const;

private:
    AudioDiscoveryManagerPrivate * m_discovery;
};

} // namespace device
} // namespace usb_cam
} // namespace nx