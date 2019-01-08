#include "buffered_stream_consumer.h"

#include "ffmpeg/packet.h"
#include "ffmpeg/frame.h"

#include "timestamp_config.h"

namespace nx {
namespace usb_cam {

namespace {

static constexpr std::chrono::milliseconds kBufferMaxTimeSpan(3000);

}

//-------------------------------------------------------------------------------------------------
// BufferedPacketConsumer

BufferedPacketConsumer::BufferedPacketConsumer(
    const std::weak_ptr<VideoStream>& streamReader,
    const CodecParameters& params)
    :
    AbstractVideoConsumer(streamReader, params)
{
}

void BufferedPacketConsumer::givePacket(const std::shared_ptr<ffmpeg::Packet>& packet)
{
    if (m_buffer.timespan() > kBufferMaxTimeSpan)
        flush();

    if (packet->mediaType() == AVMEDIA_TYPE_VIDEO && m_dropUntilNextVideoKeyPacket)
    {
        if (!packet->keyPacket())
            return;
        m_dropUntilNextVideoKeyPacket = false;
    }

    m_buffer.insert(packet->timestamp(), packet);
}

void BufferedPacketConsumer::flush()
{
    m_dropUntilNextVideoKeyPacket = true;
    m_buffer.clear();
}

std::shared_ptr<ffmpeg::Packet> BufferedPacketConsumer::popOldest(
    const std::chrono::milliseconds& timeout)
{
    return m_buffer.popOldest(timeout);
}

std::shared_ptr<ffmpeg::Packet> BufferedPacketConsumer::peekOldest(
    const std::chrono::milliseconds& timeout)
{
    return m_buffer.peekOldest(timeout);
}

void BufferedPacketConsumer::interrupt()
{
    m_buffer.interrupt();
}

bool BufferedPacketConsumer::waitForTimespan(
    const std::chrono::milliseconds& timespan,
    const std::chrono::milliseconds& timeout)
{
    return m_buffer.waitForTimespan(timespan, timeout);
}

duration_t BufferedPacketConsumer::timespan() const
{
    return m_buffer.timespan();
}

size_t BufferedPacketConsumer::size() const
{
    return m_buffer.size();
}

bool BufferedPacketConsumer::empty() const
{
    return m_buffer.empty();
}

std::vector<uint64_t> BufferedPacketConsumer::timestamps() const
{
    return m_buffer.timestamps();
}

//-------------------------------------------------------------------------------------------------
// BufferedVideoFrameConsumer

BufferedVideoFrameConsumer::BufferedVideoFrameConsumer(
    const std::weak_ptr<VideoStream>& streamReader,
    const CodecParameters& params)
    :
    AbstractVideoConsumer(streamReader, params)
{  
}

void BufferedVideoFrameConsumer::flush()
{
    m_buffer.clear();
}

void BufferedVideoFrameConsumer::giveFrame(const std::shared_ptr<ffmpeg::Frame>& frame)
{
    if (m_buffer.timespan() > kBufferMaxTimeSpan)
        flush();

    m_buffer.insert(frame->timestamp(), frame);
}

std::shared_ptr<ffmpeg::Frame> BufferedVideoFrameConsumer::popOldest(
    const std::chrono::milliseconds& timeout)
{
    return m_buffer.popOldest(timeout);
}

void BufferedVideoFrameConsumer::interrupt()
{
    m_buffer.interrupt();
}

size_t BufferedVideoFrameConsumer::size() const
{
    return m_buffer.size();
}

bool BufferedVideoFrameConsumer::empty() const
{
    return m_buffer.empty();
}

std::vector<uint64_t> BufferedVideoFrameConsumer::timestamps() const
{
    return m_buffer.timestamps();
}

} // namespace usb_cam
} // namespace nx