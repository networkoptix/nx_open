#include "stream_consumer_manager.h"

namespace nx {
namespace usb_cam {

bool AbstractStreamConsumerManager::empty() const
{
    return m_consumers.empty();
}

size_t AbstractStreamConsumerManager::size() const
{
    return m_consumers.size();
}

void AbstractStreamConsumerManager::flush()
{
    for (auto & consumer: m_consumers)
    {
        if (auto c = consumer.lock())
            c->flush();
    }
}

size_t AbstractStreamConsumerManager::addConsumer(
    const std::weak_ptr<AbstractStreamConsumer>& consumer)
{
    int index = consumerIndex(consumer);
    if (index >= m_consumers.size())
    {
        m_consumers.push_back(consumer);
        return m_consumers.size() - 1;
    }
    return -1;
}

size_t AbstractStreamConsumerManager::removeConsumer(
    const std::weak_ptr<AbstractStreamConsumer>& consumer)
{
    int index = consumerIndex(consumer);
    if (index < m_consumers.size())
    {
        m_consumers.erase(m_consumers.begin() + index);
        if(auto c = consumer.lock())
            c->flush();
        return index;
    }
    return -1;
}

int AbstractStreamConsumerManager::consumerIndex(
    const std::weak_ptr<AbstractStreamConsumer>& consumer) const
{
    int index = 0;
    for(const auto & c : m_consumers)
    {
        if (c.lock() == consumer.lock())
            return index;
        ++index;
    }
    return index;
}

int AbstractStreamConsumerManager::findLargestBitrate() const
{
    int largest = 0;
    for (const auto & consumer : m_consumers)
    {
        if (auto c = consumer.lock())
        {
            if (auto vc = std::dynamic_pointer_cast<AbstractVideoConsumer>(c))
            {
                if (largest < vc->bitrate())
                    largest = vc->bitrate();
            }
        }
    }
    return largest;
}

float AbstractStreamConsumerManager::findLargestFps() const
{
    float largest = 0;
    for (const auto & consumer : m_consumers)
    {
        if (auto c = consumer.lock())
        {
            if (auto vc = std::dynamic_pointer_cast<AbstractVideoConsumer>(c))
            {
                if (largest < vc->fps())
                    largest = vc->fps();
            }
        }
    }
    return largest;
}


nxcip::Resolution AbstractStreamConsumerManager::findLargestResolution() const
{
    nxcip::Resolution largest;
    for (const auto & consumer : m_consumers)
    {
        if (auto c = consumer.lock())
        {
            if (auto vc = std::dynamic_pointer_cast<AbstractVideoConsumer>(c))
            {
                nxcip::Resolution resolution = vc->resolution();
                if (largest.width * largest.height < resolution.width * resolution.height)
                    largest = resolution;
            }
        }
    }
    return largest;
}

//-------------------------------------------------------------------------------------------------
// FrameConsumerManager

void FrameConsumerManager::giveFrame(const std::shared_ptr<ffmpeg::Frame>& frame)
{
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

size_t PacketConsumerManager::addConsumer(const std::weak_ptr<AbstractStreamConsumer>& consumer)
{
    return addConsumer(consumer, ConsumerState::receiveEveryPacket);
}

size_t PacketConsumerManager::removeConsumer(const std::weak_ptr<AbstractStreamConsumer>& consumer)
{
    size_t index = AbstractStreamConsumerManager::removeConsumer(consumer);
    if (index != -1)
        m_consumerKeyPacketStates.erase(m_consumerKeyPacketStates.begin() + index);
    return index;
}

size_t PacketConsumerManager::addConsumer(
    const std::weak_ptr<AbstractStreamConsumer>& consumer,
    const ConsumerState& waitForKeyPacket)
{
    // This addConsumer() call needs to be the parent's to avoid recursion
    size_t index = AbstractStreamConsumerManager::addConsumer(consumer);
    if (index != -1)
    {
        if (m_consumerKeyPacketStates.empty())
            m_consumerKeyPacketStates.push_back(waitForKeyPacket);
        else
            m_consumerKeyPacketStates.insert(
                m_consumerKeyPacketStates.begin() + index,
                waitForKeyPacket);
    }

    return index;
}

void PacketConsumerManager::givePacket(const std::shared_ptr<ffmpeg::Packet>& packet)
{
    for (int i = 0; i < m_consumers.size(); ++i)
    {
        if(auto c = m_consumers[i].lock())
        {
            if(auto packetConsumer = std::dynamic_pointer_cast<AbstractPacketConsumer>(c))
            {
                if (m_consumerKeyPacketStates[i] == ConsumerState::skipUntilNextKeyPacket)
                {
                    if (!packet->keyPacket())
                        continue;
                    m_consumerKeyPacketStates[i] = ConsumerState::receiveEveryPacket;
                }
                packetConsumer->givePacket(packet);
            }
        }
    }
}

} // namespace usb_cam
} // namespace nx