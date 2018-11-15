/**********************************************************
* 03 sep 2013
* akolesnikov
***********************************************************/

#include "camera_manager.h"

#include <cstring>

#include "archive_reader.h"
#include "media_encoder.h"
#include "time_periods.h"


static const int FRAME_DURATION_USEC = 1*1000*1000;

CameraManager::CameraManager( const nxcip::CameraInfo& info )
:
    m_refManager( this ),
    m_pluginRef( ImageLibraryPlugin::instance() ),
    m_info( info ),
    m_capabilities( 
        nxcip::BaseCameraManager::dtsArchiveCapability | 
        nxcip::BaseCameraManager::nativeMediaStreamCapability |
        nxcip::BaseCameraManager::hardwareMotionCapability ),
    m_dirContentsManager(
        info.url,
        FRAME_DURATION_USEC )
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

//!Implementation of nxcip::BaseCameraManager::getEncoderCount
int CameraManager::getEncoderCount( int* encoderCount ) const
{
    *encoderCount = 2;
    return nxcip::NX_NO_ERROR;
}

//!Implementation of nxcip::BaseCameraManager::getEncoder
int CameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
{
    if( encoderIndex > 1 )
        return nxcip::NX_INVALID_ENCODER_NUMBER;

    if( !m_encoder[encoderIndex].get() )
        m_encoder[encoderIndex].reset( new MediaEncoder(this, encoderIndex, FRAME_DURATION_USEC) );
    m_encoder[encoderIndex]->addRef();
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
void CameraManager::setCredentials( const char* /*username*/, const char* /*password*/ )
{
    //TODO/IMPL
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

int CameraManager::createDtsArchiveReader( nxcip::DtsArchiveReader** dtsArchiveReader ) const
{
    *dtsArchiveReader = new ArchiveReader( &m_dirContentsManager, FRAME_DURATION_USEC );
    return nxcip::NX_NO_ERROR;
}

int CameraManager::find( nxcip::ArchiveSearchOptions* searchOptions, nxcip::TimePeriods** timePeriods ) const
{
    std::unique_ptr<TimePeriods> resTimePeriods( new TimePeriods() );
    resTimePeriods->timePeriods.push_back( std::make_pair( m_dirContentsManager.minTimestamp(), m_dirContentsManager.maxTimestamp() ) );
    *timePeriods = resTimePeriods.release();
    return nxcip::NX_NO_ERROR;
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

const DirContentsManager& CameraManager::dirContentsManager() const
{
    return m_dirContentsManager;
}

DirContentsManager* CameraManager::dirContentsManager()
{
    return &m_dirContentsManager;
}
