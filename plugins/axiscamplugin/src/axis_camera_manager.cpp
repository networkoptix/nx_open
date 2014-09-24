/**********************************************************
* 3 apr 2013
* akolesnikov
***********************************************************/

#include "axis_camera_manager.h"

#include <cstddef>
#include <cstring>

#include "axis_camera_plugin.h"
#include "axis_cam_params.h"
#include "axis_media_encoder.h"
#include "axis_relayio_manager.h"
#include "sync_http_client.h"


AxisCameraManager::AxisCameraManager( const nxcip::CameraInfo& info )
:
    m_refManager( this ),
    m_pluginRef( AxisCameraPlugin::instance() ),
    m_info( info ),
    m_audioEnabled( false ),
    m_relayIOInfoRead( false ),
    m_cameraCapabilities( 0 ),
    m_inputPortCount( 0 ),
    m_outputPortCount( 0 )
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
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

unsigned int AxisCameraManager::addRef()
{
    return m_refManager.addRef();
}

unsigned int AxisCameraManager::releaseRef()
{
    return m_refManager.releaseRef();
}

//!Implementation of nxcip::BaseCameraManager::getEncoderCount
int AxisCameraManager::getEncoderCount( int* encoderCount ) const
{
    *encoderCount = m_encoders.size();
    return nxcip::NX_NO_ERROR;
}

int AxisCameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
{
    if( encoderIndex < 0 || encoderIndex >= (int)m_encoders.size() )
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

    *capabilitiesMask = m_cameraCapabilities;
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
nxcip::CameraPtzManager* AxisCameraManager::getPtzManager() const
{
    //TODO/IMPL
    return NULL;
}

nxcip::CameraMotionDataProvider* AxisCameraManager::getCameraMotionDataProvider() const
{
    return NULL;
}

//!Implementation of nxcip::BaseCameraManager::getCameraRelayIOManager
nxcip::CameraRelayIOManager* AxisCameraManager::getCameraRelayIOManager() const
{
    //updating info, if needed
    int result = updateCameraInfo();
    if( result )
        return NULL;

    if( !((m_cameraCapabilities & nxcip::BaseCameraManager::relayInputCapability) ||
          (m_cameraCapabilities & nxcip::BaseCameraManager::relayOutputCapability)) )
    {
        return NULL;
    }

    if( !m_relayIOManager.get() )
        m_relayIOManager.reset( new AxisRelayIOManager(const_cast<AxisCameraManager*>(this), m_inputPortCount, m_outputPortCount) );
    m_relayIOManager->addRef();
    return m_relayIOManager.get();
}

//!Implementation of nxcip::BaseCameraManager::getErrorString
void AxisCameraManager::getLastErrorString( char* errorString ) const
{
    //TODO/IMPL
    errorString[0] = '\0';
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

nxpt::CommonRefManager* AxisCameraManager::refManager()
{
    return &m_refManager;
}

int AxisCameraManager::updateCameraInfo() const
{
    m_cameraCapabilities |= nxcip::BaseCameraManager::audioCapability | nxcip::BaseCameraManager::sharePixelsCapability;

    std::auto_ptr<SyncHttpClient> httpClient;
    if( std::strlen(m_info.modelName) == 0 )
    {
        if( !httpClient.get() )
            httpClient.reset( new SyncHttpClient(
                AxisCameraPlugin::instance()->networkAccessManager(),
                m_info.url,
                DEFAULT_AXIS_API_PORT,
                m_credentials ) );
        QByteArray prodShortName;
        const int status = readAxisParameter( httpClient.get(), "root.Brand.ProdShortName", &prodShortName );
        if( status != SyncHttpClient::HTTP_OK )
            return status == SyncHttpClient::HTTP_NOT_AUTHORIZED ? nxcip::NX_NOT_AUTHORIZED : nxcip::NX_OTHER_ERROR;
        prodShortName.replace( QByteArray(" "), QByteArray() );
        prodShortName.replace( QByteArray("-"), QByteArray() );
        strncpy( m_info.modelName, prodShortName.constData(), sizeof(m_info.modelName)/sizeof(*m_info.modelName)-1 );
    }

    if( std::strlen(m_info.firmware) == 0 )
    {
        //reading firmware, since it is unavailable via MDNS
        if( !httpClient.get() )
            httpClient.reset( new SyncHttpClient(
                AxisCameraPlugin::instance()->networkAccessManager(),
                m_info.url,
                DEFAULT_AXIS_API_PORT,
                m_credentials ) );

        QByteArray firmware;
        int status = readAxisParameter( httpClient.get(), "root.Properties.Firmware.Version", &firmware );
        if( status != SyncHttpClient::HTTP_OK )
            return status == SyncHttpClient::HTTP_NOT_AUTHORIZED ? nxcip::NX_NOT_AUTHORIZED : nxcip::NX_OTHER_ERROR;

        firmware = firmware.mid(firmware.indexOf('=')+1);
        strncpy( m_info.firmware, firmware.constData(), sizeof(m_info.firmware)-1 );
        m_info.firmware[sizeof(m_info.firmware)-1] = 0;
    }

    if( !m_relayIOInfoRead )
    {
        if( !httpClient.get() )
            httpClient.reset( new SyncHttpClient(
                AxisCameraPlugin::instance()->networkAccessManager(),
                m_info.url,
                DEFAULT_AXIS_API_PORT,
                m_credentials ) );

        //requesting I/O port information (if needed)
        int status = readAxisParameter( httpClient.get(), "Input.NbrOfInputs", &m_inputPortCount );
        if( status != SyncHttpClient::HTTP_OK )
            return status == SyncHttpClient::HTTP_NOT_AUTHORIZED ? nxcip::NX_NOT_AUTHORIZED : nxcip::NX_OTHER_ERROR;
        if( m_inputPortCount > 0 )
            m_cameraCapabilities |= BaseCameraManager::relayInputCapability;

        status = readAxisParameter( httpClient.get(), "Output.NbrOfOutputs", &m_outputPortCount );
        if( status != SyncHttpClient::HTTP_OK )
            return status == SyncHttpClient::HTTP_NOT_AUTHORIZED ? nxcip::NX_NOT_AUTHORIZED : nxcip::NX_OTHER_ERROR;
        if( m_outputPortCount > 0 )
            m_cameraCapabilities |= BaseCameraManager::relayOutputCapability;

        m_relayIOInfoRead = true;
    }

    return nxcip::NX_NO_ERROR;
}

int AxisCameraManager::readAxisParameter(
    SyncHttpClient* const httpClient,
    const QByteArray& paramName,
    QVariant* paramValue )
{
    if( httpClient->get( QString::fromLatin1("/axis-cgi/param.cgi?action=list&group=%1").arg(QLatin1String(paramName)).toLatin1() ) != QNetworkReply::NoError )
        return nxcip::NX_NETWORK_ERROR;

    if( httpClient->statusCode() == SyncHttpClient::HTTP_OK )
    {
        const QByteArray& body = httpClient->readWholeMessageBody();
        const QList<QByteArray>& paramItems = body.split('=');
        if( paramItems.size() == 2 && paramItems[0] == paramName )
        {
            *paramValue = QString::fromLatin1(paramItems[1]);   //have to convert to QString to enable auto conversion to int
            return SyncHttpClient::HTTP_OK;
        }
        else
        {
            return SyncHttpClient::HTTP_BAD_REQUEST;
        }
    }
    else
    {
        return httpClient->statusCode();
    }
}

int AxisCameraManager::readAxisParameter(
    SyncHttpClient* const httpClient,
    const QByteArray& paramName,
    QByteArray* paramValue )
{
    QVariant val;
    int status = readAxisParameter( httpClient, paramName, &val );
    *paramValue = val.toByteArray().trimmed();
    return status;
}

int AxisCameraManager::readAxisParameter(
    SyncHttpClient* const httpClient,
    const QByteArray& paramName,
    QString* paramValue )
{
    QVariant val;
    int status = readAxisParameter( httpClient, paramName, &val );
    *paramValue = val.toString().trimmed();
    return status;
}

int AxisCameraManager::readAxisParameter(
    SyncHttpClient* const httpClient,
    const QByteArray& paramName,
    unsigned int* paramValue )
{
    QVariant val;
    int status = readAxisParameter( httpClient, paramName, &val );
    *paramValue = val.toUInt();
    return status;
}

int AxisCameraManager::readAxisParameter(
    SyncHttpClient* const httpClient,
    const QByteArray& paramName,
    int* paramValue )
{
    QVariant val;
    int status = readAxisParameter( httpClient, paramName, &val );
    *paramValue = val.toInt();
    return status;
}
