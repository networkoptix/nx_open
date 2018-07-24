#include "media_encoder.h"

#include <nx/utils/log/log.h>
#include <nx/utils/url.h>
#include <nx/utils/app_info.h>

#include "camera_manager.h"
#include "stream_reader.h"
#include "device/utils.h"
#include "ffmpeg/utils.h"
#include "ffmpeg/stream_reader.h"

namespace nx {
namespace usb_cam {

MediaEncoder::MediaEncoder(
    int encoderIndex,
    const ffmpeg::CodecParameters& codecParams,
    CameraManager* const cameraManager,
    nxpl::TimeProvider *const timeProvider,
    const std::shared_ptr<ffmpeg::StreamReader>& ffmpegStreamReader)
:
    m_encoderIndex(encoderIndex),
    m_codecParams(codecParams),
    m_refManager(cameraManager->refManager()),
    m_cameraManager(cameraManager),
    m_timeProvider(timeProvider),
    m_ffmpegStreamReader(ffmpegStreamReader)
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
    std::string url = decodeCameraInfoUrl();

    std::vector<device::ResolutionData> list = device::getResolutionList(
        url.c_str(),
        ffmpeg::utils::toNxCompressionType(m_codecParams.codecID));

    NX_DEBUG(this) << "getResolutionList()::m_info.modelName:" << m_cameraManager->info().modelName;
    for (int i = 0; i < list.size(); ++i)
    {
        NX_DEBUG(this) << "w:" << list[i].width << ", h:" << list[i].height;
        infoList[i].resolution = {list[i].width, list[i].height};
        infoList[i].maxFps = list[i].maxFps;
    }
    *infoListCount = list.size();

    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::getMaxBitrate( int* maxBitrate ) const
{
    *maxBitrate =  device::getMaxBitrate(decodeCameraInfoUrl().c_str()) / 1000;
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setResolution( const nxcip::Resolution& resolution )
{
    m_codecParams.setResolution(resolution.width, resolution.height);
    if (m_streamReader)
        m_streamReader->setResolution(resolution);
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setFps( const float& fps, float* selectedFps )
{
    static constexpr float MIN_FPS = 1;
    static constexpr float MAX_FPS = 90;

    float newFps = std::min<float>( std::max<float>( fps, MIN_FPS ), MAX_FPS );
    m_codecParams.fps = (int) newFps;
    if( m_streamReader)
        m_streamReader->setFps((int) newFps);
    *selectedFps = newFps;
    return nxcip::NX_NO_ERROR;
}

int MediaEncoder::setBitrate( int bitrateKbps, int* selectedBitrateKbps )
{
    // the plugin uses bits per second internally, so convert to that first
    int bitratebps = bitrateKbps * 1000;
    m_codecParams.bitrate = bitratebps;
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
    if (m_streamReader)
        m_streamReader->updateCameraInfo( info );
}

int MediaEncoder::lastFfmpegError() const
{
    return m_streamReader ? m_streamReader->lastFfmpegError() : 0;
}

std::string MediaEncoder::decodeCameraInfoUrl() const
{
    QString url = QString(m_cameraManager->info().url).mid(9);
    return nx::utils::Url::fromPercentEncoding(url.toLatin1()).toStdString();
}

} // namespace nx
} // namespace usb_cam