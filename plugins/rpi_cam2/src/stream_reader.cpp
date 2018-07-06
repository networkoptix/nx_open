#include "stream_reader.h"

#include <nx/utils/log/log.h>
#include <nx/utils/url.h>

#include "utils/utils.h"
#include "ffmpeg/utils.h"
#include "ffmpeg/stream_reader.h"

namespace {
static constexpr nxcip::UsecUTCTimestamp USEC_IN_MS = 1000;
}

namespace nx {
namespace rpi_cam2 {

StreamReader::StreamReader(
    nxpt::CommonRefManager* const parentRefManager,
    nxpl::TimeProvider *const timeProvider,
    const nxcip::CameraInfo& cameraInfo,
    const CodecContext& codecContext,
    const std::shared_ptr<ffmpeg::StreamReader>& ffmpegStreamReader)
:
    m_refManager(parentRefManager),
    m_timeProvider(timeProvider),
    m_info(cameraInfo),
    m_codecContext(codecContext),
    m_ffmpegStreamReader(ffmpegStreamReader)
{
    NX_ASSERT(m_timeProvider);

    ffmpeg::CodecParameters params(
        nx::ffmpeg::utils::toAVCodecID(m_codecContext.codecID()),
        m_codecContext.fps(),
        m_codecContext.bitrate(),
        m_codecContext.resolution().width,
        m_codecContext.resolution().height);
    m_consumer.reset(new ffmpeg::BufferedStreamConsumer(params, m_ffmpegStreamReader));
}

StreamReader::~StreamReader()
{
    m_timeProvider->releaseRef();
}

void* StreamReader::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_StreamReader, sizeof(nxcip::IID_StreamReader) ) == 0 )
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

unsigned int StreamReader::addRef()
{
    return m_refManager.addRef();
}

unsigned int StreamReader::releaseRef()
{
    return m_refManager.releaseRef();
}

void StreamReader::interrupt()
{
}

void StreamReader::setFps(int fps)
{
    m_codecContext.setFps(fps);
    m_consumer->setFps(fps);
}

void StreamReader::setResolution(const nxcip::Resolution& resolution)
{
    m_codecContext.setResolution(resolution);
    m_consumer->setResolution(resolution.width, resolution.height);
}

void StreamReader::setBitrate(int bitrate)
{
    m_codecContext.setBitrate(bitrate);
    m_consumer->setBitrate(bitrate);
}

void StreamReader::updateCameraInfo( const nxcip::CameraInfo& info )
{
    m_info = info;
}

std::unique_ptr<ILPVideoPacket> StreamReader::toNxPacket(AVPacket *packet, AVCodecID codecID) 
{
    std::unique_ptr<ILPVideoPacket> nxVideoPacket(new ILPVideoPacket(
        &m_allocator,
        0,
        m_timeProvider->millisSinceEpoch() * USEC_IN_MS,
        nxcip::MediaDataPacket::fKeyPacket,
        0 ) );

    nxVideoPacket->resizeBuffer(packet->size);
    if (nxVideoPacket->data())
        memcpy(nxVideoPacket->data(), packet->data, packet->size);

    nxVideoPacket->setCodecType(ffmpeg::utils::toNxCompressionType(codecID));

    return nxVideoPacket;
}

// QString StreamReader::decodeCameraInfoUrl() const
// {
//     QString url = QString(m_info.url).mid(9);
//     return nx::utils::Url::fromPercentEncoding(url.toLatin1());
// }

// std::string StreamReader::getAVCameraUrl()
// {
//     QString s = decodeCameraInfoUrl();
//     return std::string(s.toLatin1().data());
// }

} // namespace rpi_cam2
} // namespace nx
