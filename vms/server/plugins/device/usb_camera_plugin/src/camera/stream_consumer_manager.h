#pragma once

#include <memory>
#include <vector>
#include <mutex>

#include "abstract_stream_consumer.h"
#include "ffmpeg/packet.h"

namespace nx::usb_cam {

class PacketConsumerManager
{
public:
    virtual ~PacketConsumerManager() = default;
    bool empty() const;
    size_t size() const;
    /**
     * Calls flush() on each AbstractPacketConsumer
     */
    void flush();
    void addConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer);
    void removeConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer);
    void pushPacket(const std::shared_ptr<ffmpeg::Packet>& packet);

protected:
    mutable std::mutex m_mutex;
    std::vector<std::weak_ptr<AbstractPacketConsumer>> m_consumers;
};

} // namespace nx::usb_cam
