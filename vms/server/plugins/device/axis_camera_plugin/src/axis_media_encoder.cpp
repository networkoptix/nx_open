/**********************************************************
* 3 apr 2013
* akolesnikov
***********************************************************/

#include "axis_media_encoder.h"

#include <algorithm>
#include <cstring>
#include <cstdio>

#include <QtNetwork/QAuthenticator>

#include "axis_camera_manager.h"
#include "axis_camera_plugin.h"
#include "axis_cam_params.h"
#include "sync_http_client.h"


//min known fps of axis camera (in case we do not know model)
static const float MIN_AXIS_CAMERA_FPS = 8.0;

AxisMediaEncoder::AxisMediaEncoder( AxisCameraManager* const cameraManager )
:
    m_refManager( cameraManager->refManager() ),
    m_cameraManager( cameraManager ),
    m_currentFps( 15 ),
    m_currentBitrateKbps( 0 ),
    m_maxAllowedFps( 0 ),
    m_rtspPort( -1 )
{
    m_maxAllowedFps = MIN_AXIS_CAMERA_FPS;

    for( size_t i = 0; i < sizeof(cameraFpsLimitArray) / sizeof(*cameraFpsLimitArray); ++i )
        if( std::strcmp(cameraManager->cameraInfo().modelName, cameraFpsLimitArray[i].modelName) == 0 )
        {
            m_maxAllowedFps = cameraFpsLimitArray[i].fps;
            break;
        }
}

AxisMediaEncoder::~AxisMediaEncoder()
{
}

void* AxisMediaEncoder::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraMediaEncoder, sizeof(nxcip::IID_CameraMediaEncoder) ) == 0 )
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

unsigned int AxisMediaEncoder::addRef()
{
    return m_refManager.addRef();
}

unsigned int AxisMediaEncoder::releaseRef()
{
    return m_refManager.releaseRef();
}

int AxisMediaEncoder::getMediaUrl( char* urlBuf ) const
{
    if( m_rtspPort == -1 )
    {
        SyncHttpClient http(
            AxisCameraPlugin::instance()->networkAccessManager(),
            m_cameraManager->cameraInfo().url,
            DEFAULT_AXIS_API_PORT,
            m_cameraManager->credentials() );
        const int httpStatusCode = m_cameraManager->readAxisParameter( &http, "Network.RTSP.Port", &m_rtspPort );
        if( httpStatusCode != SyncHttpClient::HTTP_OK )
            return httpStatusCode == SyncHttpClient::HTTP_NOT_AUTHORIZED ? nxcip::NX_NOT_AUTHORIZED : nxcip::NX_OTHER_ERROR;
    }

    QByteArray paramsStr;
    paramsStr.append("videocodec=h264");
    if( m_currentResolutionInfo.resolution.width * m_currentResolutionInfo.resolution.height > 0 )
        paramsStr.append("&resolution=").append(QByteArray::number(m_currentResolutionInfo.resolution.width)).
            append("x").append(QByteArray::number(m_currentResolutionInfo.resolution.height));
    paramsStr.append("&text=0"); // do not use onscreen text message (fps e.t.c)
    if( m_currentFps > 0 )
        paramsStr.append("&fps=").append(QByteArray::number(m_currentFps));
    //if( quality != QnQualityPreSet )
    //    paramsStr.append("&compression=").append(QByteArray::number(toAxisQuality(quality)));
    paramsStr.append("&audio=").append( m_cameraManager->isAudioEnabled() ? "1" : "0");

    //m_cameraManager->cameraInfo().url stores only host name
    sprintf( urlBuf, "rtsp://%s:%d/axis-media/media.amp?%s", QByteArray(m_cameraManager->cameraInfo().url).split(':').at(0).constData(), m_rtspPort, paramsStr.data() );
    return nxcip::NX_NO_ERROR;
}

int AxisMediaEncoder::getResolutionList( nxcip::ResolutionInfo* infoList, int* infoListCount ) const
{
    if( m_supportedResolutions.empty() )
    {
        int result = fetchCameraResolutionList();
        if( result != nxcip::NX_NO_ERROR )
            return result;
        if( m_supportedResolutions.empty() )
            return nxcip::NX_OTHER_ERROR;
    }

    *infoListCount = std::min<int>(
        m_supportedResolutions.size(),
        nxcip::MAX_RESOLUTION_LIST_SIZE);
    memcpy(
        infoList,
        &m_supportedResolutions[0],
        sizeof(nxcip::ResolutionInfo) * *infoListCount );

    return nxcip::NX_NO_ERROR;
}

int AxisMediaEncoder::getMaxBitrate( int* maxBitrate ) const
{
    *maxBitrate = 1000000;   //TODO/IMPL get appropriate bitrate
    return nxcip::NX_NO_ERROR;
}

struct FindResInfoByRes
{
    FindResInfoByRes( const nxcip::Resolution& resolution ) : m_resolution( resolution ) {}
    bool operator()( const nxcip::ResolutionInfo& resInfo ) const
    { return resInfo.resolution.width == m_resolution.width && resInfo.resolution.height == m_resolution.height; }

private:
    const nxcip::Resolution& m_resolution;
};

int AxisMediaEncoder::setResolution( const nxcip::Resolution& resolution )
{
    if( m_supportedResolutions.empty() )
    {
        int result = fetchCameraResolutionList();
        if( result != nxcip::NX_NO_ERROR )
            return result;
        if( m_supportedResolutions.empty() )
            return nxcip::NX_OTHER_ERROR;
    }

    std::vector<nxcip::ResolutionInfo>::const_iterator resIter = std::find_if( m_supportedResolutions.begin(), m_supportedResolutions.end(), FindResInfoByRes(resolution) );
    if( resIter == m_supportedResolutions.end() )
        return nxcip::NX_UNSUPPORTED_RESOLUTION;

    m_currentResolutionInfo = *resIter;
    return nxcip::NX_NO_ERROR;
}

int AxisMediaEncoder::setFps( const float& fps, float* selectedFps )
{
    //checking fps validity
    *selectedFps = fps > m_maxAllowedFps ? m_maxAllowedFps : fps;
    *selectedFps = fps <= 0 ? m_maxAllowedFps : fps;
    m_currentFps = *selectedFps;
    return nxcip::NX_NO_ERROR;
}

int AxisMediaEncoder::setBitrate( int bitrateKbps, int* selectedBitrateKbps )
{
    m_currentBitrateKbps = bitrateKbps;
    *selectedBitrateKbps = m_currentBitrateKbps;
    return nxcip::NX_NO_ERROR;
}

int AxisMediaEncoder::fetchCameraResolutionList() const
{
    // determin camera max resolution
    SyncHttpClient http(
        AxisCameraPlugin::instance()->networkAccessManager(),
        m_cameraManager->cameraInfo().url,
        DEFAULT_AXIS_API_PORT,
        m_cameraManager->credentials() );
    if( http.get( QLatin1String("/axis-cgi/param.cgi?action=list&group=Properties.Image.Resolution") ) != QNetworkReply::NoError )
        return nxcip::NX_NETWORK_ERROR;
    if( http.statusCode() != SyncHttpClient::HTTP_OK )
        return http.statusCode() == SyncHttpClient::HTTP_NOT_AUTHORIZED ? nxcip::NX_NOT_AUTHORIZED : nxcip::NX_OTHER_ERROR;

    const QByteArray& body = http.readWholeMessageBody();

    int paramValuePos = body.indexOf('=');
    if( paramValuePos == -1 )
        return nxcip::NX_OTHER_ERROR;

    const QList<QByteArray>& resolutionNameList = body.mid(paramValuePos+1).split(',');
    m_supportedResolutions.reserve( resolutionNameList.size() );
    for( int i = 0; i < resolutionNameList.size(); ++i )
    {
        const QByteArray& resolutionName = resolutionNameList[i].toLower().trimmed();

        m_supportedResolutions.resize( m_supportedResolutions.size()+1 );
        m_supportedResolutions[i].maxFps = m_maxAllowedFps;
        if( resolutionName == "qcif" )
        {
            m_supportedResolutions[i].resolution.width = 176;
            m_supportedResolutions[i].resolution.height = 144;
        }
        else if( resolutionName == "cif" )
        {
            m_supportedResolutions[i].resolution.width = 352;
            m_supportedResolutions[i].resolution.height = 288;
        }
        else if( resolutionName == "2cif" )
        {
            m_supportedResolutions[i].resolution.width = 704;
            m_supportedResolutions[i].resolution.height = 288;
        }
        else if( resolutionName == "4cif" )
        {
            m_supportedResolutions[i].resolution.width = 704;
            m_supportedResolutions[i].resolution.height = 576;
        }
        else if( resolutionName == "d1" )
        {
            m_supportedResolutions[i].resolution.width = 720;
            m_supportedResolutions[i].resolution.height = 576;
        }
        else
        {
            //assuming string has format 640x480
            const QList<QByteArray>& resPartsList = resolutionName.split( 'x' );
            if( resPartsList.size() == 2 )
            {
                m_supportedResolutions[i].resolution.width = resPartsList[0].toInt();
                m_supportedResolutions[i].resolution.height = resPartsList[1].toInt();
            }
            else
            {
                m_supportedResolutions.pop_back();
            }
        }
    }

    return nxcip::NX_NO_ERROR;
}
