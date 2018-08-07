#include "buffered_stream_consumer.h"

#include "packet.h"
#include "frame.h"

namespace nx {
namespace ffmpeg {


BufferedPacketConsumer::BufferedPacketConsumer(
    const std::weak_ptr<StreamReader>& streamReader,
    const CodecParameters& params)
    :
    AbstractPacketConsumer(streamReader, params),
    m_ignoreNonKeyPackets(false),
    m_interrupted(false)
{  
}

void BufferedPacketConsumer::givePacket(const std::shared_ptr<Packet>& packet)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_ignoreNonKeyPackets)
    {
        if (!packet->keyPacket())
            return;
        m_ignoreNonKeyPackets = false;
    }
    m_packets.push_back(packet);
    m_wait.notify_all();
}

std::shared_ptr<Packet> BufferedPacketConsumer::popFront()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_packets.empty())
    {
        m_wait.wait(lock, [&](){ return m_interrupted || !m_packets.empty(); });
        if(m_interrupted)
        {
            m_interrupted = false;
            return nullptr;
        }
    }

    auto packet = m_packets.front();
    m_packets.pop_front();
    return packet;
}

int BufferedPacketConsumer::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_packets.size();
}

void BufferedPacketConsumer::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_packets.clear();
}

int BufferedPacketConsumer::dropOldNonKeyPackets()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    int keepCount = 1;
    auto it = m_packets.begin();
    for (; it != m_packets.end(); ++it, ++keepCount)
    {
        if ((*it)->keyPacket())
            break;
    }

    if (it == m_packets.end())
        return 0;
        
    /* Checking if the back of the que is a key packet.
     * If it is, save it and reinsert at the back after resizing.
     * Otherwise, givePacket() drops until it gets a key packet.
     */    
    std::shared_ptr<Packet> keyPacketEnd;
    auto rit = m_packets.rbegin();
    if (rit != m_packets.rend() && (*rit)->keyPacket())
        keyPacketEnd = *rit;

    int dropCount = m_packets.size() - keepCount;
    m_packets.resize(keepCount);

    if (keyPacketEnd)
    {
        m_packets.push_back(keyPacketEnd);
        --dropCount;
    }
    else
    {
       dropUntilFirstKeyPacket();
    }

    return dropCount;
}

void BufferedPacketConsumer::interrupt()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_interrupted = true;
    m_wait.notify_all();
}

void BufferedPacketConsumer::dropUntilFirstKeyPacket()
{
    m_ignoreNonKeyPackets = true;
}


////////////////////////////////////// BufferedFrameConsumer ///////////////////////////////////////


BufferedFrameConsumer::BufferedFrameConsumer(
    const std::weak_ptr<StreamReader>& streamReader,
    const CodecParameters& params)
    :
    AbstractFrameConsumer(streamReader, params),
    m_interrupted(false)
{  
}

void BufferedFrameConsumer::giveFrame(const std::shared_ptr<Frame>& frame)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frames.push_back(frame);
    m_wait.notify_all();
}

std::shared_ptr<Frame> BufferedFrameConsumer::popFront()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_frames.empty())
    {
        m_wait.wait(lock, [&](){ return m_interrupted || !m_frames.empty(); });
        if(m_interrupted)
        {
            m_interrupted = false;
            return nullptr;
        }
    }

    auto frame = m_frames.front();
    m_frames.pop_front();
    return frame;
}

int BufferedFrameConsumer::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_frames.size();
}

void BufferedFrameConsumer::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frames.clear();
}

void BufferedFrameConsumer::interrupt()
{
    std::lock_guard<std::mutex> lock(m_mutex);
        m_interrupted = true;
    m_wait.notify_all();
}


} // namespace ffmpeg
} // namespace nx