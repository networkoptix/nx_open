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
namespace webcam_plugin {

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

} // namespace webcam_plugin
} // namespace nx
