#ifdef _WIN32

#pragma once

#include "discovery/audio_discovery_manager.h"
#include "device/device_data.h"

namespace nx {
namespace device {
 
class DShowAudioDiscoveryManager : public AudioDiscoveryManagerPrivate{
public:
    virtual void fillCameraAuxData(nxcip::CameraInfo * cameras, int cameraCount) const override;
};

} // namespace device
} // namespace nx

#endif

