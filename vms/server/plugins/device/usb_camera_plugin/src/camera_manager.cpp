
#include "camera_manager.h"

#include <cstring>

#include <nx/utils/url.h>
#include <nx/utils/log/assert.h>

#include "discovery_manager.h"
#include "native_media_encoder.h"
#include "transcode_media_encoder.h"
#include "ffmpeg/utils.h"
#include "device/video/utils.h"

namespace nx {
namespace usb_cam {

CameraManager::CameraManager(const std::shared_ptr<Camera> camera):
    m_camera(camera),
    m_refManager(this),
    m_pluginRef(Plugin::instance()),
    m_capabilities(
            nxcip::BaseCameraManager::nativeMediaStreamCapability |
            nxcip::BaseCameraManager::primaryStreamSoftMotionCapability |
            nxcip::BaseCameraManager::fixedQualityCapability |
            nxcip::BaseCameraManager::cameraTimeCapability)
{
    if (m_camera->hasAudio())
        m_capabilities |= nxcip::BaseCameraManager::audioCapability;
}

void* CameraManager::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if (memcmp(
        &interfaceID,
        &nxcip::IID_BaseCameraManager2,
        sizeof(nxcip::IID_BaseCameraManager2)) == 0)
    {
        addRef();
        return static_cast<nxcip::BaseCameraManager2*>(this);
    }
    if (memcmp(
        &interfaceID,
        &nxcip::IID_BaseCameraManager,
        sizeof(nxcip::IID_BaseCameraManager)) == 0)
    {
        addRef();
        return static_cast<nxcip::BaseCameraManager*>(this);
    }
    if (memcmp(&interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

int CameraManager::addRef() const
{
    return m_refManager.addRef();
}

int CameraManager::releaseRef() const
{
    return m_refManager.releaseRef();
}

int CameraManager::getEncoderCount(int* encoderCount) const
{
    *encoderCount = kEncoderCount;
    return nxcip::NX_NO_ERROR;
}

int CameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_camera->isInitialized() && !m_camera->initialize())
        return nxcip::NX_IO_ERROR;

    switch (encoderIndex)
    {
        case 0:
        {
            if (!m_encoders[encoderIndex])
            {
                m_encoders[encoderIndex].reset(new NativeMediaEncoder(
                    refManager(),
                    encoderIndex,
                    m_camera));
            }
            break;
        }
        case 1:
        {
            if (!m_encoders[encoderIndex])
            {
                m_encoders[encoderIndex].reset(new TranscodeMediaEncoder(
                    refManager(),
                    encoderIndex,
                    m_camera));
            }
            break;
        }
        default:
            return nxcip::NX_INVALID_ENCODER_NUMBER;
    }

    m_encoders[encoderIndex]->addRef();
    *encoderPtr = m_encoders[encoderIndex].get();

    return nxcip::NX_NO_ERROR;
}

int CameraManager::getCameraInfo( nxcip::CameraInfo* info ) const
{
    memcpy(info, &m_camera->info(), sizeof(m_camera->info()));
    return nxcip::NX_NO_ERROR;
}

int CameraManager::getCameraCapabilities( unsigned int* capabilitiesMask ) const
{
    *capabilitiesMask = m_capabilities;
    return nxcip::NX_NO_ERROR;
}

void CameraManager::setCredentials( const char* username, const char* password )
{
    m_camera->setCredentials(username, password);
}

int CameraManager::setAudioEnabled( int audioEnabled )
{
    m_camera->setAudioEnabled((bool)audioEnabled);
    return nxcip::NX_NO_ERROR;
}

nxcip::CameraPtzManager* CameraManager::getPtzManager() const
{
    return NULL;
}

nxcip::CameraMotionDataProvider* CameraManager::getCameraMotionDataProvider() const
{
    return NULL;
}

nxcip::CameraRelayIOManager* CameraManager::getCameraRelayIOManager() const
{
    return NULL;
}

void CameraManager::getLastErrorString( char* errorString ) const
{
    const auto errorToString =
        [&errorString](int ffmpegError) -> bool
        {
            bool error = ffmpegError < 0;
            if (error)
            {
                std::string s = ffmpeg::utils::errorToString(ffmpegError);
                strncpy(errorString, s.c_str(), nxcip::MAX_TEXT_LEN - 1);
            }
            return error;
        };

    if (errorToString(m_camera->lastError()))
    {
        m_camera->setLastError(0);
        return;
    }

    *errorString = '\0';
}

int CameraManager::createDtsArchiveReader( nxcip::DtsArchiveReader** /*dtsArchiveReader*/ ) const
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

int CameraManager::find( nxcip::ArchiveSearchOptions* /*searchOptions*/, nxcip::TimePeriods** /*timePeriods*/ ) const
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

int CameraManager::setMotionMask( nxcip::Picture* /*motionMask*/ )
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

nxpt::CommonRefManager* CameraManager::refManager()
{
    return &m_refManager;
}

} // namespace nx
} // namespace usb_cam
