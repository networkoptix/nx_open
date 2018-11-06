#pragma once

#include <camera/camera_plugin.h>

#include <string>

namespace nx {
namespace usb_cam {
namespace device {

namespace detail {

class AudioDiscoveryManagerPrivate
{
public:
    virtual ~AudioDiscoveryManagerPrivate() = default;
    virtual void fillCameraAuxData(nxcip::CameraInfo* cameras, int cameraCount) const = 0;
    virtual bool pluggedIn(const std::string& devicePath) const = 0;
};

} // namespace detail

class AudioDiscoveryManager
{
public:
    AudioDiscoveryManager();
    ~AudioDiscoveryManager();
    void fillCameraAuxData(nxcip::CameraInfo* cameras, int cameraCount) const;

    bool pluggedIn(const std::string& devicePath) const;

private:
    detail::AudioDiscoveryManagerPrivate * m_discovery;
};

} // namespace device
} // namespace usb_cam
} // namespace nx