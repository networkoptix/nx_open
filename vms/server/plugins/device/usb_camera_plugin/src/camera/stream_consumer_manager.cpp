#include "stream_consumer_manager.h"

namespace nx::usb_cam {

bool PacketConsumerManager::empty() const
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    return m_consumers.empty();
}

size_t PacketConsumerManager::size() const
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    return m_consumers.size();
}

void PacketConsumerManager::flush()
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    for (auto& consumer: m_consumers)
    {
        if (auto c = consumer.lock())
            c->flush();
    }
}

void PacketConsumerManager::addConsumer(
    const std::weak_ptr<AbstractPacketConsumer>& consumer)
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    m_consumers.push_back(consumer);
}

void PacketConsumerManager::removeConsumer(
    const std::weak_ptr<AbstractPacketConsumer>& consumer)
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    int index = -1;
    for (const auto& c: m_consumers)
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

void PacketConsumerManager::pushPacket(const std::shared_ptr<ffmpeg::Packet>& packet)
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    for (auto& consumer: m_consumers)
    {
        auto consumerLocked = consumer.lock();
        if (consumerLocked == consumer.lock())
            consumerLocked->pushPacket(packet);
    }

}

} // namespace nx::usb_cam
