#ifdef _WIN32

#pragma once

#include "discovery/audio_discovery_manager.h"
#include "device/device_data.h"

namespace nx {
namespace usb_cam {
namespace device {
namespace detail {
 
class DShowAudioDiscoveryManager: public AudioDiscoveryManagerPrivate
{
public:
    virtual void fillCameraAuxData(nxcip::CameraInfo * cameras, int cameraCount) const override;
    virtual bool pluggedIn(const std::string& devicePath) const override;
};

} // namespace detail
} // namespace device
} // namespace usb_cam
} // namespace nx

#endif

