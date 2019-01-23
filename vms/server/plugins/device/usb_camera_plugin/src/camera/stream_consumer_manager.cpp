#include "stream_consumer_manager.h"

namespace nx {
namespace usb_cam {

bool AbstractStreamConsumerManager::empty() const
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    return m_consumers.empty();
}

size_t AbstractStreamConsumerManager::size() const
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    return m_consumers.size();
}

void AbstractStreamConsumerManager::flush()
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    for (auto & consumer: m_consumers)
    {
        if (auto c = consumer.lock())
            c->flush();
    }
}

void AbstractStreamConsumerManager::addConsumer(
    const std::weak_ptr<AbstractStreamConsumer>& consumer)
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    m_consumers.push_back(consumer);
}

void AbstractStreamConsumerManager::removeConsumer(
    const std::weak_ptr<AbstractStreamConsumer>& consumer)
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    int index = -1;
    for(const auto & c : m_consumers)
    {
        ++index;
        auto consumerLocked = c.lock();
        if (consumerLocked == consumer.lock())
        {
            consumerLocked->flush();
            break;
        }
    }
    if (index >= 0)
        m_consumers.erase(m_consumers.begin() + index);
}

//-------------------------------------------------------------------------------------------------
// FrameConsumerManager

void FrameConsumerManager::giveFrame(const std::shared_ptr<ffmpeg::Frame>& frame)
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    for (auto & consumer : m_consumers)
    {
        if(auto c = consumer.lock())
        {
            if(auto frameConsumer = std::dynamic_pointer_cast<AbstractFrameConsumer>(c))
                frameConsumer->giveFrame(frame);
        }
    }
}

//-------------------------------------------------------------------------------------------------
// AbstractPacketConsumerManager

void PacketConsumerManager::givePacket(const std::shared_ptr<ffmpeg::Packet>& packet)
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    for (int i = 0; i < m_consumers.size(); ++i)
    {
        if(auto c = m_consumers[i].lock())
        {
            if(auto packetConsumer = std::dynamic_pointer_cast<AbstractPacketConsumer>(c))
                packetConsumer->givePacket(packet);
        }
    }
}

} // namespace usb_cam
} // namespace nx
