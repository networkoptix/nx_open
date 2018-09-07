
#include "camera_manager.h"

#include <cstring>

#include <nx/utils/url.h>
#include <nx/utils/log/assert.h>

#include "native_media_encoder.h"
#include "transcode_media_encoder.h"
#include "utils.h"
#include "ffmpeg/utils.h"
#include "device/utils.h"

namespace nx {
namespace usb_cam {

CameraManager::CameraManager(
    const nxcip::CameraInfo& info,
    nxpl::TimeProvider *const timeProvider)
:
    m_info( info ),
    m_timeProvider(timeProvider),
    m_refManager( this ),
    m_pluginRef( Plugin::instance() ),
    m_capabilities(
        nxcip::BaseCameraManager::nativeMediaStreamCapability |
        nxcip::BaseCameraManager::primaryStreamSoftMotionCapability |
        nxcip::BaseCameraManager::audioCapability),
    m_audioEnabled(false)
{
}

CameraManager::~CameraManager()
{
}

void* CameraManager::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_BaseCameraManager2, sizeof(nxcip::IID_BaseCameraManager2) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::BaseCameraManager2*>(this);
    }
    if( memcmp( &interfaceID, &nxcip::IID_BaseCameraManager, sizeof(nxcip::IID_BaseCameraManager) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::BaseCameraManager*>(this);
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

unsigned int CameraManager::addRef()
{
    return m_refManager.addRef();
}

unsigned int CameraManager::releaseRef()
{
    return m_refManager.releaseRef();
}

int CameraManager::getEncoderCount( int* encoderCount ) const
{
    *encoderCount = kEncoderCount;
    return nxcip::NX_NO_ERROR;
}

int CameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_camera)
        m_camera = std::make_shared<Camera>(m_timeProvider, m_info);

    switch(encoderIndex)
    {
        case 0:
        {
            if (!m_encoders[encoderIndex])
            {
                m_encoders[encoderIndex].reset(new NativeMediaEncoder(
                //m_encoders[encoderIndex].reset(new TranscodeMediaEncoder(
                    refManager(),
                    encoderIndex,
                    m_camera));
            }
            break;
        }
        case 1:
        {
            if(!m_encoders[encoderIndex])
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
    memcpy( info, &m_info, sizeof(m_info) );
    return nxcip::NX_NO_ERROR;
}

int CameraManager::getCameraCapabilities( unsigned int* capabilitiesMask ) const
{
    *capabilitiesMask = m_capabilities;
    return nxcip::NX_NO_ERROR;
}

void CameraManager::setCredentials( const char* username, const char* password )
{
    strncpy( m_info.defaultLogin, username, sizeof(m_info.defaultLogin)-1 );
    strncpy( m_info.defaultPassword, password, sizeof(m_info.defaultPassword)-1 );
    if (m_camera)
        m_camera->updateCameraInfo(m_info);
}

int CameraManager::setAudioEnabled( int audioEnabled )
{
    m_audioEnabled = audioEnabled;
    std::cout << "SETTING AUDIO ENABLED: " << m_audioEnabled << std::endl;
    if (m_camera)
        m_camera->setAudioEnabled(m_audioEnabled);
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
            if(error)
            {
                std::string s = ffmpeg::utils::errorToString(ffmpegError);
                strncpy(errorString, s.c_str(), nxcip::MAX_TEXT_LEN - 1);
            }
            return error;
        };

    if(m_camera && errorToString(m_camera->lastError()))
        return;

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

const nxcip::CameraInfo& CameraManager::info() const
{
    return m_info;
}

nxpt::CommonRefManager* CameraManager::refManager()
{
    return &m_refManager;
}

} // namespace nx
} // namespace usb_cam
