#pragma once

#include <memory>
#include <vector>

#include "stream_consumer.h"
#include "frame.h"
#include "packet.h"

namespace nx {
namespace ffmpeg {

///////////////////////////////////////// ConsumerManager //////////////////////////////////////////

class StreamConsumerManager
{
public:
    bool empty() const;
    size_t size() const;
    void consumerFlush();

    virtual size_t addConsumer(const std::weak_ptr<StreamConsumer>& consumer);
    virtual size_t removeConsumer(const std::weak_ptr<StreamConsumer>& consumer);
    int consumerIndex(const std::weak_ptr<StreamConsumer>& consumer) const;
    const std::vector<std::weak_ptr<StreamConsumer>>& consumers() const;

    int largestBitrate() const;
    float largestFps() const;
    void largestResolution(int * outWidth, int * outHeight) const;

protected:
    std::vector<std::weak_ptr<StreamConsumer>> m_consumers;
};

/////////////////////////////////////// FrameConsumerManager ///////////////////////////////////////

class FrameConsumerManager : public StreamConsumerManager
{
public:
    void giveFrame(const std::shared_ptr<Frame>& frame);
};

////////////////////////////////////// PacketConsumerManager ///////////////////////////////////////

class PacketConsumerManager : public StreamConsumerManager
{
public:
    virtual size_t addConsumer(const std::weak_ptr<StreamConsumer>& consumer) override;
    virtual size_t removeConsumer(const std::weak_ptr<StreamConsumer>& consumer) override;
    size_t addConsumer(const std::weak_ptr<StreamConsumer>& consumer, bool waitForKeyPacket);
    void givePacket(const std::shared_ptr<Packet>& packet);
    const std::vector<bool>& waitForKeyPacket() const;
    
private:
    /* 
    * Indexes here correspond to indexes in the parent class' vector, m_consumers.
    * If true, the consumer at a given index will not receive a packet until it is a key packet,
    * and then sets the flag to false.
    * If false, the consumer receives all packets.
    */
    std::vector<bool> m_waitForKeyPacket;
};

}  // namespace ffmpeg
} // namespace nx