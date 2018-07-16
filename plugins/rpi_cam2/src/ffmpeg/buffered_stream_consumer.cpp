#include "buffered_stream_consumer.h"

#include "packet.h"

namespace nx {
namespace ffmpeg {


BufferedStreamConsumer::BufferedStreamConsumer(
    const std::weak_ptr<StreamReader>& streamReader,
    const CodecParameters& params)
    :
    AbstractStreamConsumer(streamReader, params),
    m_ignoreNonKeyPackets(false)
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
    m_vector.push_back(packet);
}

std::shared_ptr<Packet> BufferedStreamConsumer::popFront()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_vector.empty())
        return nullptr;
    auto packet = m_vector.front();
    m_vector.pop_front();
    return packet;
}

std::shared_ptr<Packet> BufferedStreamConsumer::peekFront() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_vector.empty() ? nullptr : m_vector.front();
}

int BufferedStreamConsumer::size()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_vector.size();
}

void BufferedStreamConsumer::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_vector.clear();
}

int BufferedStreamConsumer::dropOldNonKeyPackets()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    int keepCount = 1;
    auto it = m_vector.begin();
    for (; it != m_vector.end(); ++it, ++keepCount)
    {
        if ((*it)->keyPacket())
            break;
    }

    if (it == m_vector.end())
        return 0;
        
    /* Checking if the back of the que is a key packet.
     * If it is, save it and reinsert at the back after resizing.
     * Otherwise, givePacket() drops until it gets a key packet.
     */    
    std::shared_ptr<Packet> keyPacketEnd;
    auto rit = m_vector.rbegin();
    if(rit != m_vector.rend() && (*rit)->keyPacket())
        keyPacketEnd = *rit;

    int dropCount = m_vector.size() - keepCount;
    m_vector.resize(keepCount);

    if (keyPacketEnd)
    {
        m_vector.push_back(keyPacketEnd);
        --dropCount;
    }
    else
    {
        m_ignoreNonKeyPackets = true;
    }

    return dropCount;
}

}
}