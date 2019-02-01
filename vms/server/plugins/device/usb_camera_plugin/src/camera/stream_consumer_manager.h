#pragma once

#include <memory>
#include <vector>
#include <mutex>

#include "abstract_stream_consumer.h"
#include "ffmpeg/frame.h"
#include "ffmpeg/packet.h"

namespace nx {
namespace usb_cam {

//-------------------------------------------------------------------------------------------------
// AbstractStreamConsumerManager

class AbstractStreamConsumerManager
{
public:
    virtual ~AbstractStreamConsumerManager() = default;
    bool empty() const;
    size_t size() const;
    /**
     * Calls flush() on each AbstractStreamConsumer
     */
    void flush();
    virtual void addConsumer(const std::weak_ptr<AbstractStreamConsumer>& consumer);
    virtual void removeConsumer(const std::weak_ptr<AbstractStreamConsumer>& consumer);

protected:
    mutable std::mutex m_mutex;
    std::vector<std::weak_ptr<AbstractStreamConsumer>> m_consumers;
};

//-------------------------------------------------------------------------------------------------
// FrameConsumerManager

class FrameConsumerManager: public AbstractStreamConsumerManager
{
public:
    void giveFrame(const std::shared_ptr<ffmpeg::Frame>& frame);
};

//--------------------------------------------------------------------------------------------------
// PacketConsumerManager

class PacketConsumerManager: public AbstractStreamConsumerManager
{
public:
    void givePacket(const std::shared_ptr<ffmpeg::Packet>& packet);
};

} // namespace usb_cam
} // namespace nx
