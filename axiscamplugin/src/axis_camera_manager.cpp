/**********************************************************
* 3 apr 2013
* akolesnikov
***********************************************************/

#include "axis_camera_manager.h"

#include <cstddef>

#include <utils/network/simple_http_client.h>

#include "axis_media_encoder.h"
#include "axis_cam_params.h"


AxisCameraManager::AxisCameraManager( const nxcip::CameraInfo& info )
:
    m_refCount( 1 ),
    m_info( info ),
    m_audioEnabled( false )
{
    m_credentials.setUser( QString::fromUtf8(m_info.defaultLogin) );
    m_credentials.setPassword( QString::fromUtf8(m_info.defaultPassword) );
    m_encoders.resize( 2 ); //all AXIS cameras support dual streaming
}

AxisCameraManager::~AxisCameraManager()
{
    for( std::vector<AxisMediaEncoder*>::size_type
        i = 0;
        i < m_encoders.size();
        ++i )
    {
        if( m_encoders[i] )
        {
            m_encoders[i]->releaseRef();
            m_encoders[i] = NULL;
        }
    }
}

void* AxisCameraManager::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_BaseCameraManager, sizeof(nxcip::IID_BaseCameraManager) ) == 0 )
    {
        addRef();
        return this;
    }
    return NULL;
}

unsigned int AxisCameraManager::addRef()
{
    return m_refCount.fetchAndAddOrdered(1) + 1;
}

unsigned int AxisCameraManager::releaseRef()
{
    unsigned int newRefCounter = m_refCount.fetchAndAddOrdered(-1) - 1;
    if( newRefCounter == 0 )
        delete this;
    return newRefCounter;
}

//!Implementation of nxcip::BaseCameraManager::getEncoderCount
int AxisCameraManager::getEncoderCount( int* encoderCount ) const
{
    *encoderCount = m_encoders.size();
    return nxcip::NX_NO_ERROR;
}

int AxisCameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
{
    if( encoderIndex < 0 || encoderIndex >= m_encoders.size() )
        return nxcip::NX_INVALID_ENCODER_NUMBER;

    if( !m_encoders[encoderIndex] )
        m_encoders[encoderIndex] = new AxisMediaEncoder( this );
    m_encoders[encoderIndex]->addRef();
    *encoderPtr = m_encoders[encoderIndex];
    return nxcip::NX_NO_ERROR;
}

int AxisCameraManager::getCameraInfo( nxcip::CameraInfo* info ) const
{
    int result = updateCameraInfo();
    if( result )
        return result;

    *info = m_info;
    return nxcip::NX_NO_ERROR;
}

//!Implementation of nxcip::BaseCameraManager::getCameraCapabilities
int AxisCameraManager::getCameraCapabilities( unsigned int* capabilitiesMask ) const
{
    //updating info, if needed
    int result = updateCameraInfo();
    if( result )
        return result;

    *capabilitiesMask = nxcip::BaseCameraManager::audioCapability | nxcip::BaseCameraManager::sharePixelsCapability;
    return nxcip::NX_NO_ERROR;
}

//!Implementation of nxcip::BaseCameraManager::setCredentials
void AxisCameraManager::setCredentials( const char* username, const char* password )
{
    if( username )
        m_credentials.setUser( QString::fromUtf8(username) );
    if( password )
        m_credentials.setPassword( QString::fromUtf8(password) );
}

//!Implementation of nxcip::BaseCameraManager::setAudioEnabled
int AxisCameraManager::setAudioEnabled( int audioEnabled )
{
    m_audioEnabled = audioEnabled != 0;
    return nxcip::NX_NO_ERROR;
}

//!Implementation of nxcip::BaseCameraManager::getPTZManager
nxcip::CameraPTZManager* AxisCameraManager::getPTZManager() const
{
    //TODO/IMPL
    return NULL;
}

//!Implementation of nxcip::BaseCameraManager::getCameraMotionDataProvider
nxcip::CameraMotionDataProvider* AxisCameraManager::getCameraMotionDataProvider() const
{
    //TODO/IMPL
    return NULL;
}

//!Implementation of nxcip::BaseCameraManager::getCameraRelayIOManager
nxcip::CameraRelayIOManager* AxisCameraManager::getCameraRelayIOManager() const
{
    //TODO/IMPL
    return NULL;
}

//!Implementation of nxcip::BaseCameraManager::getErrorString
void AxisCameraManager::getErrorString( int errorCode, char* errorString ) const
{
    //TODO/IMPL
}

const nxcip::CameraInfo& AxisCameraManager::cameraInfo() const
{
    return m_info;
}

nxcip::CameraInfo& AxisCameraManager::cameraInfo()
{
    return m_info;
}

const QAuthenticator& AxisCameraManager::credentials() const
{
    return m_credentials;
}

bool AxisCameraManager::isAudioEnabled() const
{
    return m_audioEnabled;
}

int AxisCameraManager::updateCameraInfo() const
{
    if( std::strlen(m_info.firmware) == 0 )
    {
        CLSimpleHTTPClient http( m_info.url, DEFAULT_AXIS_API_PORT, DEFAULT_SOCKET_READ_WRITE_TIMEOUT_MS, m_credentials );
        CLHttpStatus status = http.doGET( QByteArray("axis-cgi/param.cgi?action=list&group=root.Properties.Firmware.Version") );
        if( status != CL_HTTP_SUCCESS )
            return status == CL_HTTP_AUTH_REQUIRED ? nxcip::NX_NOT_AUTHORIZED : nxcip::NX_OTHER_ERROR;

        QByteArray firmware;
        http.readAll( firmware );
        firmware = firmware.mid(firmware.indexOf('=')+1);
        const int firmwareLen = std::min<int>(sizeof(m_info.firmware)-1, firmware.size());
        strncpy( m_info.firmware, firmware.data(), firmwareLen );
        m_info.firmware[firmwareLen] = 0;
    }

    //TODO/IMPL requesting I/O port information (if needed)

    return nxcip::NX_NO_ERROR;
}
