#include "media_encoder.h"

#include <nx/utils/log/log.h>
#include <nx/utils/url.h>

#include "camera_manager.h"
#include "stream_reader.h"
#include "utils/utils.h"
#include "ffmpeg/stream_reader.h"

namespace nx {
namespace rpi_cam2 {

MediaEncoder::MediaEncoder(
    CameraManager* const cameraManager,
    nxpl::TimeProvider *const timeProvider,
    const CodecContext& codecContext,
    const std::shared_ptr<nx::ffmpeg::StreamReader>& ffmpegStreamReader)
:
    m_refManager( cameraManager->refManager() ),
    m_cameraManager( cameraManager ),
    m_timeProvider( timeProvider ),
    m_videoCodecContext(codecContext),
    m_ffmpegStreamReader(ffmpegStreamReader),
    m_maxBitrate(0)
{
}

MediaEncoder::~MediaEncoder()
{
}

void* MediaEncoder::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_CameraMediaEncoder2, sizeof(nxcip::IID_CameraMediaEncoder2) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::CameraMediaEncoder2*>(this);
    }
    if( memcmp( &interfaceID, &nxcip::IID_CameraMediaEncoder, sizeof(nxcip::IID_CameraMediaEncoder) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::CameraMediaEncoder*>(this);
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

unsigned int MediaEncoder::addRef()
{
    return m_refManager.addRef();
}

unsigned int MediaEncoder::releaseRef()
{
    return m_refManager.releaseRef();
}

int MediaEncoder::getMediaUrl( char* urlBuf ) const
{
    strcpy( urlBuf, m_cameraManager->info().url );
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::getResolutionList( nxcip::ResolutionInfo* infoList, int* infoListCount ) const
{
    QString url = decodeCameraInfoUrl();

    std::vector<utils::ResolutionData> resList
        = utils::getResolutionList(url.toLatin1().data(), m_videoCodecContext.codecID());

    NX_DEBUG(this) << "getResolutionList()::m_info.modelName:" << m_cameraManager->info().modelName;
    int count = resList.size();
    for (int i = 0; i < count; ++i)
    {
        NX_DEBUG(this)
            << "w:" << resList[i].resolution.width << ", h:" << resList[i].resolution.height;
        infoList[i].resolution = resList[i].resolution;
        infoList[i].maxFps = resList[i].maxFps;
    }
    *infoListCount = count;
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::getMaxBitrate( int* maxBitrate ) const
{
    if(m_maxBitrate)
    {
        *maxBitrate = m_maxBitrate / 1000;
        return nxcip::NX_NO_ERROR;
    }

    QString url = decodeCameraInfoUrl();

    std::vector<utils::ResolutionData> resList
        = utils::getResolutionList(url.toLatin1().data(), m_videoCodecContext.codecID());

    auto max = std::max_element(resList.begin(), resList.end(),
        [](const utils::ResolutionData& a, const utils::ResolutionData& b)
        {
            return a.bitrate < b.bitrate;
        });

    m_maxBitrate = max != resList.end() ? (int) max->bitrate : 0;
    *maxBitrate = m_maxBitrate / 1000;
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setResolution( const nxcip::Resolution& resolution )
{
    m_videoCodecContext.setResolution(resolution);
    if (m_streamReader)
        m_streamReader->setResolution(resolution);
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setFps( const float& fps, float* selectedFps )
{
    static constexpr double MIN_FPS = 1.0 / 86400.0; //once per day
    static constexpr double MAX_FPS = 90;

    float newFps = std::min<float>( std::max<float>( fps, MIN_FPS ), MAX_FPS );
    m_videoCodecContext.setFps((int) newFps);
    if( m_streamReader)
        m_streamReader->setFps((int) newFps);
    *selectedFps = newFps;
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setBitrate( int bitrateKbps, int* selectedBitrateKbps )
{
    // the plugin uses bits per second internally, so convert to that first
    int bitratebps = bitrateKbps * 1000;
    m_videoCodecContext.setBitrate(bitratebps);
    if (m_streamReader)
        m_streamReader->setBitrate(bitratebps);
    *selectedBitrateKbps = bitrateKbps;
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::getAudioFormat( nxcip::AudioFormat* audioFormat ) const
{
    static_cast<void>( audioFormat );
    return nxcip::NX_UNSUPPORTED_CODEC;
}

void MediaEncoder::updateCameraInfo( const nxcip::CameraInfo& info )
{
    if (m_streamReader.get())
        m_streamReader->updateCameraInfo( info );
}

void MediaEncoder::setVideoCodecID (nxcip::CompressionType codecID)
{
    m_videoCodecContext.setCodecID(codecID);
}

QString MediaEncoder::decodeCameraInfoUrl() const
{
    QString url = QString(m_cameraManager->info().url).mid(9);
    return nx::utils::Url::fromPercentEncoding(url.toLatin1());
}


} // namespace nx
} // namespace rpi_cam2