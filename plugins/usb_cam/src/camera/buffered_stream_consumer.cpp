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

void BufferedPacketConsumer::pushBack(const std::shared_ptr<ffmpeg::Packet>& packet)
{
    if (m_ignoreNonKeyPackets)
    {
        if (!packet->keyPacket())
            return;
        m_ignoreNonKeyPackets = false;
    }
    Buffer::pushBack(packet);
}

int BufferedPacketConsumer::dropOldNonKeyPackets()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    int keepCount = 1;
    auto it = m_buffer.begin();
    for (; it != m_buffer.end(); ++it, ++keepCount)
    {
        if ((*it)->keyPacket())
            break;
    }

    if (it == m_buffer.end())
        return 0;
        
    /* Checking if the back of the que is a key packet.
     * If it is, save it and reinsert at the back after resizing.
     * Otherwise, givePacket() drops until it gets a key packet.
     */    
    std::shared_ptr<ffmpeg::Packet> keyPacketEnd;
    auto rit = m_buffer.rbegin();
    if (rit != m_buffer.rend() && (*rit)->keyPacket())
        keyPacketEnd = *rit;

    int dropCount = m_buffer.size() - keepCount;
    m_buffer.resize(keepCount);

    if (keyPacketEnd)
    {
        m_buffer.push_back(keyPacketEnd);
        --dropCount;
    }
    else
    {
       dropUntilFirstKeyPacket();
    }
    
    return dropCount;
}

void BufferedPacketConsumer::dropUntilFirstKeyPacket()
{
    m_ignoreNonKeyPackets = true;
}


//////////////////////////////////// BufferedVideoPacketConsumer ///////////////////////////////////

BufferedVideoPacketConsumer::BufferedVideoPacketConsumer(
    const std::weak_ptr<VideoStream>& streamReader,
    const CodecParameters& params)
    :
    AbstractVideoConsumer(streamReader, params)
{  
}


/////////////////////////////////// BufferedVideoFrameConsumer /////////////////////////////////////

BufferedVideoFrameConsumer::BufferedVideoFrameConsumer(
    const std::weak_ptr<VideoStream>& streamReader,
    const CodecParameters& params)
    :
    AbstractVideoConsumer(streamReader, params)
{  
}

} // namespace usb_cam
} // namespace nx