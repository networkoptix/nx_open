#include "native_stream_reader.h"

#include <memory>

#include <nx/utils/log/log.h>

#include "ffmpeg/stream_reader.h"
#include "ffmpeg/codec.h"
#include "ffmpeg/utils.h"
#include "ffmpeg/packet.h"

namespace nx {
namespace usb_cam {

NativeStreamReader::NativeStreamReader(
    int encoderIndex,
    nxpl::TimeProvider *const timeProvider,
    const ffmpeg::CodecParameters& codecParams,
    const std::shared_ptr<nx::ffmpeg::StreamReader>& ffmpegStreamReader)
:
    InternalStreamReader(
        encoderIndex,
        timeProvider,
        codecParams,
        ffmpegStreamReader),
        m_consumer(new ffmpeg::BufferedPacketConsumer(ffmpegStreamReader, codecParams)),
        m_added(false),
        m_interrupted(false)
{
}

NativeStreamReader::~NativeStreamReader()
{
    m_ffmpegStreamReader->removePacketConsumer(m_consumer);
}

int NativeStreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    *lpPacket = nullptr;

    ensureAdded();
    maybeDropPackets();

    auto packet = m_consumer->popFront();

    if(m_interrupted || !packet)
    {
        m_interrupted = false;
        return nxcip::NX_INTERRUPTED;
    }

    /*!
     * Windows build of ffmpeg, or maybe just 3.1.9, doesn't set AV_PACKET_KEY_FLAG on packets produced
     * by av_read_frame(). To get around this, we must force the packet to be a key packet so the
     * client can attempt to decode it.
     */ 
#ifdef _WIN32
    bool forceKeyPacket = true;
#else
    bool forceKeyPacket = false;
#endif

    *lpPacket = toNxPacket(
        packet->packet(),
        packet->codecID(),
        packet->timeStamp() * 1000,
        forceKeyPacket).release();

    return nxcip::NX_NO_ERROR;
}

void NativeStreamReader::interrupt()
{
    m_interrupted = true;
    m_added = false;
    m_consumer->interrupt();
    m_consumer->clear();
    m_ffmpegStreamReader->removePacketConsumer(m_consumer);
}

void NativeStreamReader::setFps(float fps)
{
    InternalStreamReader::setFps(fps);
    m_consumer->setFps(fps);
}
void NativeStreamReader::setResolution(const nxcip::Resolution& resolution)
{
    InternalStreamReader::setResolution(resolution);
    m_consumer->setResolution(resolution.width, resolution.height);
}

void NativeStreamReader::setBitrate(int bitrate)
{
    InternalStreamReader::setBitrate(bitrate);
    m_consumer->setBitrate(bitrate);
}


void NativeStreamReader::ensureAdded()
{
    if (!m_added)
    {
        m_added = true;
        m_consumer->dropUntilFirstKeyPacket();
        m_ffmpegStreamReader->addPacketConsumer(m_consumer);
    }
}

void NativeStreamReader::maybeDropPackets()
{
    if (m_consumer->size() >= m_codecParams.fps)
    {
        int dropped = m_consumer->dropOldNonKeyPackets();
        NX_DEBUG(this) << m_ffmpegStreamReader->url() + ":"
            << "InternalStreamReader " << m_encoderIndex 
            << " dropping " << dropped << "packets.";
    }
}

} // namespace usb_cam
} // namespace nx
