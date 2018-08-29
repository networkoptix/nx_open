#include "audio_discovery_manager.h"

#ifdef _WIN32
#include "dshow/dshow_audio_discovery_manager.h"
#else // __linux__
#include "alsa/alsa_audio_discovery_manager.h"
#endif


namespace nx {
namespace device {

////////////////////////////////////// AudioDiscoveryManager ///////////////////////////////////////

AudioDiscoveryManager::AudioDiscoveryManager()
{
#ifdef _WIN32
    m_discovery = new DShowAudioDiscoveryManager();
#else // __linux__
    m_discovery = new AlsaAudioDiscoveryManager();
#endif
}

AudioDiscoveryManager::~AudioDiscoveryManager()
{
    delete m_discovery;
}

void AudioDiscoveryManager::fillCameraAuxData(nxcip::CameraInfo* cameras, int cameraCount) const
{
    m_discovery->fillCameraAuxData(cameras, cameraCount);
}

} // namespace device
} // namespace nx