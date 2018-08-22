#include "native_stream_reader.h"

#include <memory>

#include <nx/utils/log/log.h>

#include "ffmpeg/codec.h"
#include "ffmpeg/utils.h"
#include "ffmpeg/packet.h"

namespace nx {
namespace usb_cam {

NativeStreamReader::NativeStreamReader(
    int encoderIndex,
    const CodecParameters& codecParams,
    const std::shared_ptr<Camera>& camera)
:
    StreamReaderPrivate(
        encoderIndex,
        codecParams,
        camera),
        m_consumer(new BufferedVideoPacketConsumer(camera->videoStreamReader(), codecParams))
{
    std::cout << "NativeStreamReader" << std::endl;
}

NativeStreamReader::~NativeStreamReader()
{
    std::cout << "~NativeStreamReader" << std::endl;
    m_camera->videoStreamReader()->removePacketConsumer(m_consumer);
    m_camera->audioStreamReader()->removePacketConsumer(m_audioConsumer);
}

int NativeStreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    *lpPacket = nullptr;

    ensureConsumerAdded();
    maybeDropPackets();

    nxcip::DataPacketType mediaType = nxcip::dptEmpty;
    auto packet = nextPacket(&mediaType);
    if (m_interrupted)
    {
        m_interrupted = false;
        return nxcip::NX_INTERRUPTED;
    }

    if(!packet)
        return nxcip::NX_OTHER_ERROR;

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
        mediaType,
        packet->timeStamp() * 1000,
        forceKeyPacket).release();

    return nxcip::NX_NO_ERROR;
}

void NativeStreamReader::interrupt()
{
    StreamReaderPrivate::interrupt();
    m_camera->videoStreamReader()->removePacketConsumer(m_consumer);
    m_consumer->interrupt();
    m_consumer->flush();
}

void NativeStreamReader::setFps(float fps)
{
    StreamReaderPrivate::setFps(fps);
    m_consumer->setFps(fps);
}
void NativeStreamReader::setResolution(const nxcip::Resolution& resolution)
{
    StreamReaderPrivate::setResolution(resolution);
    m_consumer->setResolution(resolution.width, resolution.height);
}

void NativeStreamReader::setBitrate(int bitrate)
{
    StreamReaderPrivate::setBitrate(bitrate);
    m_consumer->setBitrate(bitrate);
}


void NativeStreamReader::ensureConsumerAdded()
{
    if (!m_consumerAdded)
    {
        StreamReaderPrivate::ensureConsumerAdded();
        m_camera->videoStreamReader()->addPacketConsumer(m_consumer);
    }
}

void NativeStreamReader::maybeDropPackets()
{
    if (m_consumer->size() >= m_codecParams.fps)
    {
        int dropped = m_consumer->dropOldNonKeyPackets();
        NX_DEBUG(this) << m_camera->videoStreamReader()->url() + ":"
            << "StreamReaderPrivate " << m_encoderIndex << " dropping" << dropped << "packets.";
    }

    if(m_audioConsumer->size() >= 60)
    {
        int dropped = m_audioConsumer->size();
        m_audioConsumer->flush();
        NX_DEBUG(this) << m_camera->videoStreamReader()->url() << "," 
            << m_camera->audioStreamReader()->url() << "dropping " << dropped << "audio packets";
    }
}

std::shared_ptr<ffmpeg::Packet> NativeStreamReader::nextPacket(nxcip::DataPacketType * outMediaType)
{
    const auto audioPacket = m_audioConsumer->peekFront();
    const auto videoPacket = m_consumer->peekFront();

    if (audioPacket && videoPacket)
    {
        if (audioPacket->timeStamp() <= videoPacket->timeStamp())
        {
            *outMediaType = nxcip::dptAudio;
            return m_audioConsumer->popFront();
        }
        else
        {
            *outMediaType = nxcip::dptVideo;
            return m_consumer->popFront();
        }
    }
    else if(audioPacket && !videoPacket)
    {
        *outMediaType = nxcip::dptAudio;
        return m_audioConsumer->popFront();
    }
    else
    {
        *outMediaType = nxcip::dptVideo;
        return m_consumer->popFront();
    }
}

} // namespace usb_cam
} // namespace nx