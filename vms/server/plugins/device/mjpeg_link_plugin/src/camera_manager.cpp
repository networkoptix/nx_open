#include "camera_manager.h"

#include <cstring>

#include "media_encoder.h"

namespace nx::vms_server_plugins::mjpeg_link {

CameraManager::CameraManager(const nxcip::CameraInfo& info,
                             nxpl::TimeProvider *const timeProvider)
:
    m_refManager( this ),
    m_pluginRef( HttpLinkPlugin::instance() ),
    m_info( info ),
    m_capabilities(
        nxcip::BaseCameraManager::nativeMediaStreamCapability |
        nxcip::BaseCameraManager::primaryStreamSoftMotionCapability |
        nxcip::BaseCameraManager::shareIpCapability |
        nxcip::BaseCameraManager::customMediaUrlCapability |
        nxcip::BaseCameraManager::fixedQualityCapability),
    m_timeProvider(timeProvider)
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

int CameraManager::addRef() const
{
    return m_refManager.addRef();
}

int CameraManager::releaseRef() const
{
    return m_refManager.releaseRef();
}

//!Implementation of nxcip::BaseCameraManager::getEncoderCount
int CameraManager::getEncoderCount( int* encoderCount ) const
{
    *encoderCount = kEncoderCount;
    return nxcip::NX_NO_ERROR;
}

//!Implementation of nxcip::BaseCameraManager::getEncoder
int CameraManager::getEncoder( int index, nxcip::CameraMediaEncoder** encoderPtr )
{
    if (index != 0 && index != 1)
        return nxcip::NX_INVALID_ENCODER_NUMBER;

    if( !m_encoder[index].get() )
        m_encoder[index].reset( new MediaEncoder(this, m_timeProvider, index) );
    m_encoder[index]->addRef();
    *encoderPtr = m_encoder[index].get();

    return nxcip::NX_NO_ERROR;
}

//!Implementation of nxcip::BaseCameraManager::getCameraInfo
int CameraManager::getCameraInfo( nxcip::CameraInfo* info ) const
{
    memcpy( info, &m_info, sizeof(m_info) );
    return nxcip::NX_NO_ERROR;
}

//!Implementation of nxcip::BaseCameraManager::getCameraCapabilities
int CameraManager::getCameraCapabilities( unsigned int* capabilitiesMask ) const
{
    *capabilitiesMask = m_capabilities;
    return nxcip::NX_NO_ERROR;
}

//!Implementation of nxcip::BaseCameraManager::setCredentials
void CameraManager::setCredentials( const char* username, const char* password )
{
    strncpy( m_info.defaultLogin, username, sizeof(m_info.defaultLogin)-1 );
    strncpy( m_info.defaultPassword, password, sizeof(m_info.defaultPassword)-1 );
    for (int i = 0; i < kEncoderCount; ++i)
    {
        if (m_encoder[i])
            m_encoder[i]->updateCredentials(username, password);
    }
}

//!Implementation of nxcip::BaseCameraManager::setAudioEnabled
int CameraManager::setAudioEnabled( int /*audioEnabled*/ )
{
    return nxcip::NX_NO_ERROR;
}

//!Implementation of nxcip::BaseCameraManager::getPTZManager
nxcip::CameraPtzManager* CameraManager::getPtzManager() const
{
    return NULL;
}

//!Implementation of nxcip::BaseCameraManager::getCameraMotionDataProvider
nxcip::CameraMotionDataProvider* CameraManager::getCameraMotionDataProvider() const
{
    return NULL;
}

//!Implementation of nxcip::BaseCameraManager::getCameraRelayIOManager
nxcip::CameraRelayIOManager* CameraManager::getCameraRelayIOManager() const
{
    return NULL;
}

//!Implementation of nxcip::BaseCameraManager::getLastErrorString
void CameraManager::getLastErrorString( char* errorString ) const
{
    errorString[0] = '\0';
    //TODO/IMPL
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

} // namespace nx::vms_server_plugins::mjpeg_link
