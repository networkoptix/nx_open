#include "stream_reader.h"

#include <nx/utils/url.h>
#include <nx/utils/log/log_main.h>

#include "ffmpeg/utils.h"
#include "ffmpeg/stream_reader.h"
#include "ffmpeg/buffered_stream_consumer.h"
#include "native_stream_reader.h"
#include "transcode_stream_reader.h"
#include "nx/utils/app_info.h"

namespace nx {
namespace usb_cam {

StreamReader::StreamReader(
    int encoderIndex,
    nxpl::TimeProvider *const timeProvider,
    const ffmpeg::CodecParameters& codecParams,
    const std::shared_ptr<ffmpeg::StreamReader>& ffmpegStreamReader,
    nxpt::CommonRefManager* const parentRefManager)
    :
    m_timeProvider(timeProvider),
    m_refManager(parentRefManager)
{
    if (codecParams.codecID == AV_CODEC_ID_NONE) // needs transcoding to a supported codec
        m_streamReader.reset(new TranscodeStreamReader(
        encoderIndex,
        timeProvider,
        codecParams,
        ffmpegStreamReader));
    else
        m_streamReader.reset(new NativeStreamReader(
        encoderIndex,
        timeProvider,
        codecParams,
        ffmpegStreamReader));
}

StreamReader::StreamReader(
    nxpl::TimeProvider *const timeProvider,
    nxpt::CommonRefManager* const parentRefManager,
    std::unique_ptr<InternalStreamReader>& streamReader)
    :
    m_timeProvider(timeProvider),
    m_refManager(parentRefManager),
    m_streamReader(std::move(streamReader))
{
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

int StreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    return m_streamReader->getNextData(lpPacket);
}

void StreamReader::interrupt()
{
    m_streamReader->interrupt();
}

void StreamReader::setFps(int fps)
{
    m_streamReader->setFps(fps);
}

void StreamReader::setResolution(const nxcip::Resolution& resolution)
{
    m_streamReader->setResolution(resolution);
}

void StreamReader::setBitrate(int bitrate)
{
    m_streamReader->setBitrate(bitrate);
}

int StreamReader::lastFfmpegError() const
{
    return m_streamReader->lastFfmpegError();
}

//////////////////////////////////// InternalStreamReader /////////////////////////////////////////


InternalStreamReader::InternalStreamReader(
    int encoderIndex,
    nxpl::TimeProvider * const timeProvider,
    const ffmpeg::CodecParameters &codecParams,
    const std::shared_ptr<ffmpeg::StreamReader> &ffmpegStreamReader)
    :
    m_encoderIndex(encoderIndex),
    m_timeProvider(timeProvider),
    m_codecParams(codecParams),
    m_ffmpegStreamReader(ffmpegStreamReader),
    m_added(false),
    m_interrupted(false),
    m_lastFfmpegError(0)
{
    NX_ASSERT(timeProvider);
    m_consumer.reset(new ffmpeg::BufferedStreamConsumer(m_ffmpegStreamReader, m_codecParams));
}

InternalStreamReader::~InternalStreamReader()
{
    m_ffmpegStreamReader->removeConsumer(m_consumer);
}


void InternalStreamReader::interrupt()
{
    m_interrupted = true;
    m_consumer->interrupt();
    m_added = false;
    m_ffmpegStreamReader->removeConsumer(m_consumer);
}

void InternalStreamReader::setFps(int fps)
{
    m_codecParams.fps = fps;
    m_consumer->setFps(fps);
}

void InternalStreamReader::setResolution(const nxcip::Resolution& resolution)
{
    m_codecParams.setResolution(resolution.width, resolution.height);
    m_consumer->setResolution(resolution.width, resolution.height);
}

void InternalStreamReader::setBitrate(int bitrate)
{
    m_codecParams.bitrate = bitrate;
    m_consumer->setBitrate(bitrate);
}

int InternalStreamReader::lastFfmpegError() const
{
    return m_lastFfmpegError;
}

std::unique_ptr<ILPVideoPacket> InternalStreamReader::toNxPacket(
    AVPacket *packet,
    AVCodecID codecID,
    uint64_t timeUsec,
    bool forceKeyPacket)
{
    int keyPacket;
    if (forceKeyPacket)
        keyPacket = nxcip::MediaDataPacket::fKeyPacket;
    else
        keyPacket = (packet->flags & AV_PKT_FLAG_KEY) ? nxcip::MediaDataPacket::fKeyPacket : 0;

    std::unique_ptr<ILPVideoPacket> nxVideoPacket(new ILPVideoPacket(
        &m_allocator,
        0,
        timeUsec,
        keyPacket,
        0));

    nxVideoPacket->resizeBuffer(packet->size);
    if (nxVideoPacket->data())
        memcpy(nxVideoPacket->data(), packet->data, packet->size);

    nxVideoPacket->setCodecType(ffmpeg::utils::toNxCompressionType(codecID));

    return nxVideoPacket;
}

std::shared_ptr<ffmpeg::Packet> InternalStreamReader::nextPacket()
{
    return m_consumer->popFront();
}

void InternalStreamReader::maybeDropPackets()
{
    int dropLimit = m_ffmpegStreamReader->gopSize();
    if (dropLimit == 1)
    {
        m_consumer->clear();
        return;
    }
    else if (!dropLimit || dropLimit > 30)
        dropLimit = std::min(std::max(dropLimit, 60), (int)m_codecParams.fps);

    if (dropLimit && m_consumer->size() >= dropLimit)
    {
        int dropped = m_consumer->dropOldNonKeyPackets();
        NX_DEBUG(this) << m_ffmpegStreamReader->url() + ":"
            << "InternalStreamReader " << m_encoderIndex 
            << " dropping " << dropped << "packets.";
    }
}

void InternalStreamReader::ensureAdded()
{
    if (!m_added)
    {
        m_added = true;
        m_consumer->ignoreNonKeyPackets();
        m_ffmpegStreamReader->addConsumer(m_consumer);
    }
}

} // namespace usb_cam
} // namespace nx
