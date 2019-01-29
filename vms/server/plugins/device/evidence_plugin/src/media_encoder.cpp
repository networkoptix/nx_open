/**********************************************************
* 3 apr 2013
* akolesnikov
***********************************************************/

#include "media_encoder.h"

#include <algorithm>
#include <cstring>
#include <cstdio>

#include <QtNetwork/QAuthenticator>

#include "camera_manager.h"
#include "camera_plugin.h"
#include "cam_params.h"
#include "sync_http_client.h"


//min known fps of axis camera (in case we do not know model)
static const float MIN_AXIS_CAMERA_FPS = 8.0;
static const int HI_STREAM_ENCODER_INDEX = 0;
static const int LO_STREAM_ENCODER_INDEX = 1;

MediaEncoder::MediaEncoder( CameraManager* const cameraManager, int encoderNum )
:
    m_encoderNum( encoderNum ),
    m_refManager( cameraManager->refManager() ),
    m_cameraManager( cameraManager ),
    m_currentFps( 15 ),
    m_currentBitrateKbps( 0 ),
    m_maxAllowedFps( 0 )
{
    m_maxAllowedFps = MIN_AXIS_CAMERA_FPS;

    for( size_t i = 0; i < sizeof(cameraFpsLimitArray) / sizeof(*cameraFpsLimitArray); ++i )
        if( std::strcmp(cameraManager->cameraInfo().modelName, cameraFpsLimitArray[i].modelName) == 0 )
        {
            m_maxAllowedFps = cameraFpsLimitArray[i].fps;
            break;
        }
}

MediaEncoder::~MediaEncoder()
{
}

void* MediaEncoder::queryInterface( const nxpl::NX_GUID& interfaceID )
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

int MediaEncoder::addRef() const
{
    return m_refManager.addRef();
}

int MediaEncoder::releaseRef() const
{
    return m_refManager.releaseRef();
}

int MediaEncoder::getMediaUrl( char* urlBuf ) const
{
    int rtspPort = 0;
    const int result = m_cameraManager->getRtspPort( rtspPort );
    if( result )
        return result;

    QUrl rtspUrl(m_cameraManager->cameraInfo().url);
    sprintf( urlBuf, "rtsp://%s:%d/%s",
        rtspUrl.host().toLatin1().constData(),
        rtspPort,
        m_encoderNum == 0 ? "h264" : "h264_2" );
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::getResolutionList( nxcip::ResolutionInfo* infoList, int* infoListSize ) const
{
    const std::vector<nxcip::ResolutionInfo>* resolutionList = nullptr;
    const int result = m_encoderNum == HI_STREAM_ENCODER_INDEX
        ? m_cameraManager->hiStreamResolutions( resolutionList )
        : m_cameraManager->loStreamResolutions( resolutionList );
    if( result != nxcip::NX_NO_ERROR )
        return result;

    *infoListSize = std::min<int>(
        resolutionList->size(),
        nxcip::MAX_RESOLUTION_LIST_SIZE);
    if( resolutionList->size() > 0 )
        memcpy(
            infoList,
            &resolutionList->at(0),
            sizeof(nxcip::ResolutionInfo) * *infoListSize );

    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::getMaxBitrate( int* maxBitrate ) const
{
    const int result = m_encoderNum == HI_STREAM_ENCODER_INDEX
        ? m_cameraManager->hiStreamMaxBitrateKbps( *maxBitrate )
        : m_cameraManager->loStreamMaxBitrateKbps( *maxBitrate );
    return result;
}

struct FindResInfoByRes
{
    FindResInfoByRes( const nxcip::Resolution& resolution ) : m_resolution( resolution ) {}
    bool operator()( const nxcip::ResolutionInfo& resInfo ) const
    { return resInfo.resolution.width == m_resolution.width && resInfo.resolution.height == m_resolution.height; }

private:
    const nxcip::Resolution& m_resolution;
};

int MediaEncoder::setResolution( const nxcip::Resolution& resolution )
{
    return m_cameraManager->setResolution( m_encoderNum, resolution );
}

int MediaEncoder::setFps( const float& fps, float* selectedFps )
{
#if 0
    //checking fps validity
    *selectedFps = fps > m_maxAllowedFps ? m_maxAllowedFps : fps;
    *selectedFps = fps <= 0 ? m_maxAllowedFps : fps;
    m_currentFps = *selectedFps;
#endif
    *selectedFps = fps;
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setBitrate( int bitrateKbps, int* selectedBitrateKbps )
{
    //TODO/IMPL set bitrate
        //root.Image.I0.Appearance.H264Bitrate=5478
        //root.Image.I0.Appearance.H264_2Bitrate=192

    SyncHttpClient http(
        CameraPlugin::instance()->networkAccessManager(),
        m_cameraManager->cameraInfo().url,
        DEFAULT_CAMERA_API_PORT,
        m_cameraManager->credentials() );

    const int result = CameraManager::updateCameraParameter(
        &http,
        QByteArray("Image.I0.Appearance.H264")+(m_encoderNum > 0 ? "_2" : "")+"Bitrate",
        QByteArray::number(bitrateKbps) );
    if( result != nxcip::NX_NO_ERROR )
        return result;

    m_currentBitrateKbps = bitrateKbps;
    *selectedBitrateKbps = m_currentBitrateKbps;
    return nxcip::NX_NO_ERROR;
}
