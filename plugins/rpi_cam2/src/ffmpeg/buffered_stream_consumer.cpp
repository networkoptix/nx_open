#include "buffered_stream_consumer.h"

#include "packet.h"

#include "../utils/utils.h"

namespace nx {
namespace ffmpeg {


BufferedStreamConsumer::BufferedStreamConsumer(
    const std::weak_ptr<StreamReader>& streamReader,
    const CodecParameters& params)
    :
    AbstractStreamConsumer(streamReader, params)
{  
}

void BufferedStreamConsumer::givePacket(const std::shared_ptr<Packet>& packet)
{
    std::lock_guard<std::mutex> lock(m_mutex);
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
    std::lock_guard<std::mutex>lock(m_mutex);
    int keepCount = 1;
    auto it = m_vector.begin();
    for (; it != m_vector.end(); ++it, ++keepCount)
        if ((*it)->keyFrame())
            break;

    if (it != m_vector.end())
    {
        int dropCount = m_vector.size() - keepCount;
        m_vector.resize(keepCount);
        return dropCount;
    }

    return 0;
}

}
}