#include "buffered_stream_consumer.h"

#include "ffmpeg/packet.h"
#include "ffmpeg/frame.h"

namespace nx {
namespace usb_cam {

//--------------------------------------------------------------------------------------------------
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
    m_buffer.insert(packet->timestamp(), packet);
}

void BufferedPacketConsumer::flush()
{
    m_buffer.clear();
}

std::shared_ptr<ffmpeg::Packet> BufferedPacketConsumer::popOldest(
    const std::chrono::milliseconds& timeOut)
{
    return m_buffer.popOldest(timeOut);
}

std::shared_ptr<ffmpeg::Packet> BufferedPacketConsumer::peekOldest(
    const std::chrono::milliseconds& timeOut)
{
    return m_buffer.peekOldest(timeOut);
}

void BufferedPacketConsumer::interrupt()
{
    m_buffer.interrupt();
}

bool BufferedPacketConsumer::waitForTimeSpan(
    const std::chrono::milliseconds& timeSpan,
    const std::chrono::milliseconds& timeOut)
{
    return m_buffer.waitForTimeSpan(timeSpan, timeOut);
}

std::chrono::milliseconds BufferedPacketConsumer::timeSpan() const
{
    return m_buffer.timeSpan();
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

//--------------------------------------------------------------------------------------------------
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
    m_buffer.insert(frame->timestamp(), frame);
}

std::shared_ptr<ffmpeg::Frame> BufferedVideoFrameConsumer::popOldest(
    const std::chrono::milliseconds& timeOut)
{
    return m_buffer.popOldest(timeOut);
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