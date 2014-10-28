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
    m_capabilities |= nxcip::BaseCameraManager::audioCapability;
    m_capabilities |= nxcip::BaseCameraManager::shareIpCapability | nxcip::BaseCameraManager::primaryStreamSoftMotionCapability;
}

GenericRTSPCameraManager::~GenericRTSPCameraManager()
{
}

void* GenericRTSPCameraManager::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    if( memcmp( &interfaceID, &nxcip::IID_BaseCameraManager, sizeof(nxcip::IID_BaseCameraManager) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::BaseCameraManager*>(this);
    }
    if( memcmp( &interfaceID, &nxcip::IID_BaseCameraManager2, sizeof(nxcip::IID_BaseCameraManager2) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::BaseCameraManager2*>(this);
    }
    if( memcmp( &interfaceID, &nxcip::IID_BaseCameraManager3, sizeof(nxcip::IID_BaseCameraManager3) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::BaseCameraManager3*>(this);
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
void GenericRTSPCameraManager::setCredentials( const char* /*username*/, const char* /*password*/ )
{
    //TODO/IMPL
}

//!Implementation of nxcip::BaseCameraManager::setAudioEnabled
int GenericRTSPCameraManager::setAudioEnabled( int /*audioEnabled*/ )
{
    //TODO/IMPL
    return 0;
}

//!Implementation of nxcip::BaseCameraManager::getPTZManager
nxcip::CameraPtzManager *GenericRTSPCameraManager::getPtzManager() const
{
    return NULL;
}

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


int GenericRTSPCameraManager::createDtsArchiveReader( nxcip::DtsArchiveReader** /*dtsArchiveReader*/ ) const
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

int GenericRTSPCameraManager::find( nxcip::ArchiveSearchOptions* /*searchOptions*/, nxcip::TimePeriods** /*timePeriods*/ ) const
{
    return nxcip::NX_NOT_IMPLEMENTED;
}

int GenericRTSPCameraManager::setMotionMask( nxcip::Picture* /*motionMask*/ )
{
    return nxcip::NX_NOT_IMPLEMENTED;
}


const char* GenericRTSPCameraManager::getParametersDescriptionXML() const
{
    static const char* rtspPluginParamsXML = "\
        <cameras>                                                                                                   \
            <camera name=\"rtsp_cam\">                                                                              \
                <group name=\"main\">                                                                               \
                    <param name=\"rtsp_url\" description=\"RTSP URL\" dataType=\"String\" readOnly=\"true\"/>       \
                </group>                                                                                            \
            </camera>                                                                                               \
        </cameras>                                                                                                  \
        ";

    return rtspPluginParamsXML;
}

int GenericRTSPCameraManager::getParamValue( const char* paramName, char* valueBuf, int* valueBufSize ) const
{
    if( strcmp(paramName, "/main/rtsp_url") == 0 )
    {
        const int requiredBufSize = strlen( m_info.url )+1;
        if( *valueBufSize < requiredBufSize )
        {
            *valueBufSize = requiredBufSize;
            return nxcip::NX_MORE_DATA;
        }
        *valueBufSize = requiredBufSize;
        strcpy( valueBuf, m_info.url );
        return nxcip::NX_NO_ERROR;
    }

    return nxcip::NX_UNKNOWN_PARAMETER;
}

int GenericRTSPCameraManager::setParamValue( const char* paramName, const char* /*value*/ )
{
    if( strcmp(paramName, "/main/rtsp_url") == 0 )
        return nxcip::NX_PARAM_READ_ONLY;

    return nxcip::NX_UNKNOWN_PARAMETER;
}


const nxcip::CameraInfo& GenericRTSPCameraManager::info() const
{
    return m_info;
}

nxpt::CommonRefManager* GenericRTSPCameraManager::refManager()
{
    return &m_refManager;
}
