/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#include "camera_manager.h"

#include <cstring>

#include "media_encoder.h"
#include <iostream>


CameraManager::CameraManager( const nxcip::CameraInfo& info )
:
    m_refManager( this ),
    m_pluginRef( IsdNativePlugin::instance() ),
    m_info( info ),
    m_capabilities( 
        nxcip::BaseCameraManager::nativeMediaStreamCapability |
        //nxcip::BaseCameraManager::shareFpsCapability |
#ifndef NO_ISD_AUDIO
	nxcip::BaseCameraManager::audioCapability |
#endif
        nxcip::BaseCameraManager::hardwareMotionCapability),
    m_motionMask( nullptr ),
    m_audioEnabled( false )
{
#ifndef NO_ISD_AUDIO
    m_audioStreamReader.reset( new AudioStreamReader() );
#endif
}

CameraManager::~CameraManager()
{
    if (m_motionMask)
        m_motionMask->releaseRef();

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

//!Implementation of nxcip::BaseCameraManager::getEncoderCount
int CameraManager::getEncoderCount( int* encoderCount ) const
{
    *encoderCount = 2;
    return nxcip::NX_NO_ERROR;
}

//!Implementation of nxcip::BaseCameraManager::getEncoder
int CameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
{
    //std::cout << "get encoder #" << encoderIndex << std::endl;

    if( encoderIndex > 1 )
        return nxcip::NX_INVALID_ENCODER_NUMBER;

    if( !m_encoder[encoderIndex].get() )
    {
        m_encoder[encoderIndex].reset( new MediaEncoder(
            this,
            encoderIndex
#ifndef NO_ISD_AUDIO
            , m_audioStreamReader
#endif
            ) );
        m_encoder[encoderIndex]->setAudioEnabled( m_audioEnabled );
    }
    m_encoder[encoderIndex]->addRef();
    if (m_motionMask)
        m_encoder[encoderIndex]->setMotionMask(m_motionMask);
    *encoderPtr = m_encoder[encoderIndex].get();

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
    if( username )
        strcpy( m_info.defaultLogin, username );
    if( password )
        strcpy( m_info.defaultPassword, password );
}

//!Implementation of nxcip::BaseCameraManager::setAudioEnabled
int CameraManager::setAudioEnabled( int audioEnabled )
{
#ifndef NO_ISD_AUDIO
    if( (audioEnabled == m_audioEnabled) && !m_audioEnabled )
        return nxcip::NX_NO_ERROR;

    if( !m_audioStreamReader->initializeIfNeeded() )
        return nxcip::NX_IO_ERROR;

    m_audioEnabled = audioEnabled;
    if (m_encoder[0].get())
        m_encoder[0]->setAudioEnabled(m_audioEnabled);
    if (m_encoder[1].get())
        m_encoder[1]->setAudioEnabled(m_audioEnabled);
    return nxcip::NX_NO_ERROR;
#else
    return nxcip::NX_NOT_IMPLEMENTED;
#endif
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

int CameraManager::createDtsArchiveReader( nxcip::DtsArchiveReader** dtsArchiveReader ) const
{
    *dtsArchiveReader = 0;
    return nxcip::NX_NOT_IMPLEMENTED;
}

int CameraManager::find( nxcip::ArchiveSearchOptions* /*searchOptions*/, nxcip::TimePeriods** /*timePeriods*/ ) const
{
    /*
    std::auto_ptr<TimePeriods> resTimePeriods( new TimePeriods() );
    resTimePeriods->timePeriods.push_back( std::make_pair( m_dirContentsManager.minTimestamp(), m_dirContentsManager.maxTimestamp() ) );
    *timePeriods = resTimePeriods.release();
    */
    return 0;
}

int CameraManager::setMotionMask( nxcip::Picture* motionMask )
{
    //TODO/IMPL
    if (m_motionMask)
        m_motionMask->releaseRef();
    m_motionMask = motionMask;
    motionMask->addRef();
    if (m_encoder[0].get())
        m_encoder[0]->setMotionMask(m_motionMask);
    if (m_encoder[1].get())
        m_encoder[1]->setMotionMask(m_motionMask);

    return nxcip::NX_NO_ERROR;
}

const nxcip::CameraInfo& CameraManager::info() const
{
    return m_info;
}

nxpt::CommonRefManager* CameraManager::refManager()
{
    return &m_refManager;
}
