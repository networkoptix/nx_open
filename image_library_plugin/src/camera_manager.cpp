/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#include "camera_manager.h"

#include <cstring>

#include <QString>
#include <QStringList>

#include "media_encoder.h"


CameraManager::CameraManager( const nxcip::CameraInfo& info )
:
    m_refManager( this ),
    m_pluginRef( ImageLibraryPlugin::instance() ),
    m_info( info ),
    m_capabilities( 0 )
{
}

CameraManager::~CameraManager()
{
}

void* CameraManager::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_BaseCameraManager, sizeof(nxcip::IID_BaseCameraManager) ) == 0 )
    {
        addRef();
        return this;
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

//!Implementation of nxcip::BaseCameraManager::getEncoderCount
int CameraManager::getEncoderCount( int* encoderCount ) const
{
    *encoderCount = 1;
    return nxcip::NX_NO_ERROR;
}

//!Implementation of nxcip::BaseCameraManager::getEncoder
int CameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
{
    if( encoderIndex != 0 )
        return nxcip::NX_INVALID_ENCODER_NUMBER;

    if( !m_encoder.get() )
        m_encoder.reset( new MediaEncoder(this) );
    m_encoder->addRef();
    *encoderPtr = m_encoder.get();

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
    //TODO/IMPL
}

//!Implementation of nxcip::BaseCameraManager::setAudioEnabled
int CameraManager::setAudioEnabled( int audioEnabled )
{
    //TODO/IMPL
    return 0;
}

//!Implementation of nxcip::BaseCameraManager::getPTZManager
nxcip::CameraPTZManager* CameraManager::getPTZManager() const
{
    return NULL;
}

//!Implementation of nxcip::BaseCameraManager::getCameraRelayIOManager
nxcip::CameraRelayIOManager* CameraManager::getCameraRelayIOManager() const
{
    return NULL;
}

int CameraManager::createDtsArchiveReader( nxcip::DtsArchiveReader** dtsArchiveReader ) const
{
    //TODO/IMPL
    return nxcip::NX_NOT_IMPLEMENTED;
}

//!Implementation of nxcip::BaseCameraManager::getLastErrorString
void CameraManager::getLastErrorString( char* errorString ) const
{
    errorString[0] = '\0';
    //TODO/IMPL
}

const nxcip::CameraInfo& CameraManager::info() const
{
    return m_info;
}

CommonRefManager* CameraManager::refManager()
{
    return &m_refManager;
}
