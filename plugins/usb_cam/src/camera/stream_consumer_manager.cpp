#include "stream_consumer_manager.h"

namespace nx {
namespace usb_cam {

bool StreamConsumerManager::empty() const
{
    return m_consumers.empty();
}

size_t StreamConsumerManager::size() const
{
    return m_consumers.size();
}

void StreamConsumerManager::consumerFlush()
{
    for (auto & consumer: m_consumers)
    {
        if (auto c = consumer.lock())
            c->flush();
    }
}

size_t StreamConsumerManager::addConsumer(const std::weak_ptr<StreamConsumer>& consumer)
{
    int index = consumerIndex(consumer);
    if (index >= m_consumers.size())
    {
        m_consumers.push_back(consumer);
        return m_consumers.size() - 1;
    }
    return -1;
}

size_t StreamConsumerManager::removeConsumer(const std::weak_ptr<StreamConsumer>& consumer)
{
    int index = consumerIndex(consumer);
    if (index < m_consumers.size())
    {
        m_consumers.erase(m_consumers.begin() + index);
        return index;
    }
    return -1;
}

int StreamConsumerManager::consumerIndex(const std::weak_ptr<StreamConsumer>& consumer) const
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

const std::vector<std::weak_ptr<StreamConsumer>>& StreamConsumerManager::consumers() const
{
    return m_consumers;
}

std::weak_ptr<VideoConsumer> StreamConsumerManager::largestBitrate(int * outBitrate) const
{
    int largest = 0;
    std::weak_ptr<VideoConsumer> videoConsumer;
    for (const auto & consumer : m_consumers)
    {
        if (auto c = consumer.lock())
        {
            if (auto vc = std::dynamic_pointer_cast<VideoConsumer>(c))
            {
                if (largest < vc->bitrate())
                {
                    largest = vc->bitrate();
                    videoConsumer = vc;
                }
            }
        }
    }
    if (outBitrate)
        *outBitrate = largest;
    return videoConsumer;
}

std::weak_ptr<VideoConsumer> StreamConsumerManager::largestFps(float * outFps) const
{
    float largest = 0;
    std::weak_ptr<VideoConsumer> videoConsumer;
    for (const auto & consumer : m_consumers)
    {
        if (auto c = consumer.lock())
        {
            if (auto vc = std::dynamic_pointer_cast<VideoConsumer>(c))
            {
                if (largest < vc->fps())
                {
                    largest = vc->fps();
                    videoConsumer = vc;
                }
            }
        }
    }
    if (outFps)
        *outFps = largest;
    return videoConsumer;
}


std::weak_ptr<VideoConsumer> StreamConsumerManager::largestResolution(int * outWidth, int * outHeight) const
{
    int largestWidth = 0;
    int largestHeight = 0;
    std::weak_ptr<VideoConsumer> videoConsumer;
    for (const auto & consumer : m_consumers)
    {
        if (auto c = consumer.lock())
        {
            if (auto vc = std::dynamic_pointer_cast<VideoConsumer>(c))
            {
                int width;
                int height;
                vc->resolution(&width, &height);
                if (largestWidth * largestHeight < width * height)
                {
                    largestWidth = width;
                    largestHeight = height;
                    videoConsumer = vc;
                }
            }
        }
    }
    if (outWidth)
        *outWidth = largestWidth;
    if (outHeight)
        *outHeight = largestHeight;
    return videoConsumer;
}

/////////////////////////////////////// FrameConsumerManager ///////////////////////////////////////

void FrameConsumerManager::giveFrame(const std::shared_ptr<ffmpeg::Frame>& frame)
{
    for (auto & consumer : m_consumers)
    {
        if(auto c = consumer.lock())
        {
            if(auto frameConsumer = std::dynamic_pointer_cast<FrameConsumer>(c))
                frameConsumer->giveFrame(frame);
        }
    }
}

////////////////////////////////////// PacketConsumerManager ///////////////////////////////////////

size_t PacketConsumerManager::addConsumer(const std::weak_ptr<StreamConsumer>& consumer)
{
    return addConsumer(consumer, false/*waitForKeyPacket*/);
}

size_t PacketConsumerManager::removeConsumer(const std::weak_ptr<StreamConsumer>& consumer)
{
    size_t index = StreamConsumerManager::removeConsumer(consumer);
    if (index != -1)
        m_waitForKeyPacket.erase(m_waitForKeyPacket.begin() + index);
    return index;
}

size_t PacketConsumerManager::addConsumer(
    const std::weak_ptr<StreamConsumer>& consumer,
    bool waitForKeyPacket)
{
    /* this addConsumer() call needs to be the parent's to avoid recursion*/
    size_t index = StreamConsumerManager::addConsumer(consumer);
    if (index != -1)
    {
        if (m_waitForKeyPacket.empty())
            m_waitForKeyPacket.push_back(waitForKeyPacket);
        else
            m_waitForKeyPacket.insert(m_waitForKeyPacket.begin() + index, waitForKeyPacket);
    }
    return index;
}

void PacketConsumerManager::givePacket(const std::shared_ptr<ffmpeg::Packet>& packet)
{
    for (int i = 0; i < m_consumers.size(); ++i)
    {
        if(auto c = m_consumers[i].lock())
        {
            if(auto packetConsumer = std::dynamic_pointer_cast<PacketConsumer>(c))
            {
                if (m_waitForKeyPacket[i])
                {
                    if (!packet->keyPacket())
                        continue;
                    m_waitForKeyPacket[i] = false;
                }
                packetConsumer->givePacket(packet);
            }
        }
    }
}

} // namespace usb_cam
} // namespace nx