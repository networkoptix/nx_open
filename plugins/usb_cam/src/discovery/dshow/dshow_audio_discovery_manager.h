#ifdef _WIN32

#pragma once

#include "../audio_discovery_manager.h"

namespace nx {
namespace device {
 
class DShowAudioDiscoveryManager : public AudioDiscoveryManagerPrivate
{
public:
    DShowAudioDiscoveryManager();
    ~DShowAudioDiscoveryManager();
    virtual void fillCameraAuxData(nxcip::CameraInfo * cameras, int cameraCount) const override;
};

} // namespace device
} // namespace nx

#endif

