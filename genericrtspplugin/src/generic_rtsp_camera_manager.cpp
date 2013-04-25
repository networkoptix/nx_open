/**********************************************************
* 22 apr 2013
* akolesnikov
***********************************************************/

#include "generic_rtsp_camera_manager.h"

#include <cstring>

#include <QString>
#include <QStringList>

#include "generic_rtsp_media_encoder.h"


GenericRTSPCameraManager::GenericRTSPCameraManager( const nxcip::CameraInfo& info )
:
    m_refManager( this ),
    m_pluginRef( GenericRTSPPlugin::instance() ),
    m_info( info ),
    m_capabilities( 0 )
{
    //checking, whether nxcip::audioCapability is supported
    QString auxiliaryData = QString::fromLatin1(m_info.auxiliaryData);
    const QStringList& valList = auxiliaryData.split( QLatin1Char('='), QString::SkipEmptyParts );
    if( !valList.isEmpty() && valList[0] == QLatin1String("audio") )
        m_capabilities |= nxcip::BaseCameraManager::audioCapability;
}

GenericRTSPCameraManager::~GenericRTSPCameraManager()
{
}

void* GenericRTSPCameraManager::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_BaseCameraManager, sizeof(nxcip::IID_BaseCameraManager) ) == 0 )
    {
        addRef();
        return this;
    }
    return NULL;
}

unsigned int GenericRTSPCameraManager::addRef()
{
    return m_refManager.addRef();
}

unsigned int GenericRTSPCameraManager::releaseRef()
{
    return m_refManager.releaseRef();
}

//!Implementation of nxcip::BaseCameraManager::getEncoderCount
int GenericRTSPCameraManager::getEncoderCount( int* encoderCount ) const
{
    *encoderCount = 1;
    return nxcip::NX_NO_ERROR;
}

//!Implementation of nxcip::BaseCameraManager::getEncoder
int GenericRTSPCameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
{
    if( encoderIndex != 0 )
        return nxcip::NX_INVALID_ENCODER_NUMBER;

    if( !m_encoder.get() )
        m_encoder.reset( new GenericRTSPMediaEncoder(this) );
    m_encoder->addRef();
    *encoderPtr = m_encoder.get();

    return nxcip::NX_NO_ERROR;
}

//!Implementation of nxcip::BaseCameraManager::getCameraInfo
int GenericRTSPCameraManager::getCameraInfo( nxcip::CameraInfo* info ) const
{
    memcpy( info, &m_info, sizeof(m_info) );
    return nxcip::NX_NO_ERROR;
}

//!Implementation of nxcip::BaseCameraManager::getCameraCapabilities
int GenericRTSPCameraManager::getCameraCapabilities( unsigned int* capabilitiesMask ) const
{
    *capabilitiesMask = m_capabilities;
    return nxcip::NX_NO_ERROR;
}

//!Implementation of nxcip::BaseCameraManager::setCredentials
void GenericRTSPCameraManager::setCredentials( const char* username, const char* password )
{
    //TODO/IMPL
}

//!Implementation of nxcip::BaseCameraManager::setAudioEnabled
int GenericRTSPCameraManager::setAudioEnabled( int audioEnabled )
{
    //TODO/IMPL
    return 0;
}

//!Implementation of nxcip::BaseCameraManager::getPTZManager
nxcip::CameraPTZManager* GenericRTSPCameraManager::getPTZManager() const
{
    return NULL;
}

//!Implementation of nxcip::BaseCameraManager::getCameraMotionDataProvider
nxcip::CameraMotionDataProvider* GenericRTSPCameraManager::getCameraMotionDataProvider() const
{
    return NULL;
}

//!Implementation of nxcip::BaseCameraManager::getCameraRelayIOManager
nxcip::CameraRelayIOManager* GenericRTSPCameraManager::getCameraRelayIOManager() const
{
    return NULL;
}

//!Implementation of nxcip::BaseCameraManager::getLastErrorString
void GenericRTSPCameraManager::getLastErrorString( char* errorString ) const
{
    errorString[0] = '\0';
    //TODO/IMPL
}

const nxcip::CameraInfo& GenericRTSPCameraManager::info() const
{
    return m_info;
}

CommonRefManager* GenericRTSPCameraManager::refManager()
{
    return &m_refManager;
}
