#include "buffered_stream_consumer.h"

#include "packet.h"

namespace nx {
namespace ffmpeg {


BufferedStreamConsumer::BufferedStreamConsumer(
    const std::weak_ptr<StreamReader>& streamReader,
    const CodecParameters& params)
    :
    AbstractStreamConsumer(streamReader, params),
    m_ignoreNonKeyPackets(false),
    m_waiting(false),
    m_interrupted(false)
{  
}

void BufferedStreamConsumer::givePacket(const std::shared_ptr<Packet>& packet)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_ignoreNonKeyPackets)
    {
        if (!packet->keyPacket())
            return;
        m_ignoreNonKeyPackets = false;
    }
    m_packets.push_back(packet);
    m_wait.notify_one();
}

std::shared_ptr<Packet> BufferedStreamConsumer::popFront()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_packets.empty())
    {
        m_waiting = true;
        m_wait.wait(lock, [&](){ return m_interrupted || !m_packets.empty(); });
        if(m_interrupted)
        {
            m_interrupted = false;
            m_waiting = false;
            return nullptr;
        }
    }

    auto packet = m_packets.front();
    m_packets.pop_front();
    return packet;
}

int BufferedStreamConsumer::size()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_packets.size();
}

void BufferedStreamConsumer::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_packets.clear();
}

int BufferedStreamConsumer::dropOldNonKeyPackets()
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
        m_ignoreNonKeyPackets = true;
    }

    return dropCount;
}

void BufferedStreamConsumer::interrupt()
{
    //if(m_waiting)
        m_interrupted = true;
}

} // namespace ffmpeg
} // namespace nx