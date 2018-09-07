#include "buffered_stream_consumer.h"

#include "ffmpeg/packet.h"
#include "ffmpeg/frame.h"

namespace nx {
namespace usb_cam {

//////////////////////////////////////// BufferedPacketConsumer ////////////////////////////////////

BufferedPacketConsumer::BufferedPacketConsumer():
    m_ignoreNonKeyPackets(false)
{
}

void BufferedPacketConsumer::pushBack(const uint64_t& timeStamp, const std::shared_ptr<ffmpeg::Packet>& packet)
{
    if (m_ignoreNonKeyPackets)
    {
        if (!packet->keyPacket())
            return;
        m_ignoreNonKeyPackets = false;
    }
    Map::pushBack(timeStamp, packet);
}

int BufferedPacketConsumer::dropOldNonKeyPackets()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    int keepCount = 1;
    auto it = m_buffer.begin();
    for (; it != m_buffer.end(); ++it, ++keepCount)
    {
        if (it->second->keyPacket())
            break;
    }

    if (it == m_buffer.end())
        return 0;
        
    /* Checking if the back of the que is a key packet.
     * If it is, save it.
     * Otherwise, givePacket() drops until it gets a key packet.
     */    
    std::shared_ptr<ffmpeg::Packet> keyPacketEnd;
    auto rit = m_buffer.rbegin();
    if (rit != m_buffer.rend() && rit->second->keyPacket())
        keyPacketEnd = rit->second;

    int dropCount = m_buffer.size() - keepCount;
    dropCount = keyPacketEnd ? dropCount - 1 : dropCount;
    for (int i = 0; i < dropCount; ++i)
    {
        if(it->second->mediaType() == AVMEDIA_TYPE_VIDEO)
            it = m_buffer.erase(it);
    }

    if (!keyPacketEnd)
        dropUntilFirstKeyPacket();
    
    return dropCount;
}

void BufferedPacketConsumer::dropUntilFirstKeyPacket()
{
    m_ignoreNonKeyPackets = true;
}

void BufferedPacketConsumer::flush()
{
    clear();
}

void BufferedPacketConsumer::givePacket(const std::shared_ptr<ffmpeg::Packet>& packet)
{
    pushBack(packet->timeStamp(), packet);
}

bool BufferedPacketConsumer::waitForTimeStampDifference(uint64_t difference)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_wait.wait(lock,
        [&]()
    {
        if (!m_buffer.empty())
        {
            uint64_t oldest = m_buffer.begin()->first;
            uint64_t newest = m_buffer.rbegin()->first;
            return newest - oldest >= difference;
        }
        return m_interrupted;
    });
    return !interrupted();
}

uint64_t BufferedPacketConsumer::timeSpan() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_buffer.empty())
        return 0;

    return m_buffer.rbegin()->first - m_buffer.begin()->first;
}


///////////////////////////////// BufferedAudioVideoPacketConsumer ////////////////////////////////

BufferedAudioVideoPacketConsumer::BufferedAudioVideoPacketConsumer(
    const std::weak_ptr<VideoStream>& streamReader,
    const CodecParameters& params)
    :
    AbstractVideoConsumer(streamReader, params),
    m_videoCount(0),
    m_audioCount(0)
{
}

void BufferedAudioVideoPacketConsumer::pushBack(const uint64_t & timeStamp, const std::shared_ptr<ffmpeg::Packet>& packet)
{
    BufferedPacketConsumer::pushBack(timeStamp, packet);
    if (packet)
    {
        if (packet->mediaType() == AVMEDIA_TYPE_VIDEO)
            ++m_videoCount;
        else
            ++m_audioCount;
    }
}

std::shared_ptr<ffmpeg::Packet> BufferedAudioVideoPacketConsumer::popFront(uint64_t timeOut)
{
    auto packet = BufferedPacketConsumer::popFront(timeOut);
    if (packet) // could be null if interrupted while waiting or timeout
    {
        if (packet->mediaType() == AVMEDIA_TYPE_VIDEO)
            --m_videoCount;
        else
            --m_audioCount;
    }
    return packet;
}

int BufferedAudioVideoPacketConsumer::videoCount() const
{
    return m_videoCount;
}
int BufferedAudioVideoPacketConsumer::audioCount() const
{
    return m_audioCount;
}

/////////////////////////////////// BufferedVideoFrameConsumer ////////////////////////////////////

BufferedVideoFrameConsumer::BufferedVideoFrameConsumer(
    const std::weak_ptr<VideoStream>& streamReader,
    const CodecParameters& params)
    :
    AbstractVideoConsumer(streamReader, params)
{  
}

void BufferedVideoFrameConsumer::flush()
{
    clear();
}

void BufferedVideoFrameConsumer::giveFrame(const std::shared_ptr<ffmpeg::Frame>& frame)
{
    pushBack(frame->timeStamp(), frame);
 //   pushBack(frame);
}

} // namespace usb_cam
} // namespace nx