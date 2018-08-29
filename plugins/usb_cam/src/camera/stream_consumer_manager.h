#pragma once

#include <memory>
#include <vector>

#include "stream_consumer.h"
#include "ffmpeg/frame.h"
#include "ffmpeg/packet.h"

namespace nx {
namespace usb_cam {


////////////////////////////////////// StreamConsumerManager ///////////////////////////////////////

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

    std::weak_ptr<VideoConsumer> largestBitrate(int * outBitrate = nullptr) const;
    std::weak_ptr<VideoConsumer> largestFps(float * outFps = nullptr) const;
    std::weak_ptr<VideoConsumer> largestResolution(int * outWidth = nullptr, int * outHeight = nullptr) const;

protected:
    std::vector<std::weak_ptr<StreamConsumer>> m_consumers;
};


/////////////////////////////////////// FrameConsumerManager ///////////////////////////////////////

class FrameConsumerManager : public StreamConsumerManager
{
public:
    void giveFrame(const std::shared_ptr<ffmpeg::Frame>& frame);
};


////////////////////////////////////// PacketConsumerManager ///////////////////////////////////////

class PacketConsumerManager : public StreamConsumerManager
{
public:
    virtual size_t addConsumer(const std::weak_ptr<StreamConsumer>& consumer) override;
    virtual size_t removeConsumer(const std::weak_ptr<StreamConsumer>& consumer) override;
    size_t addConsumer(const std::weak_ptr<StreamConsumer>& consumer, bool waitForKeyPacket);
    void givePacket(const std::shared_ptr<ffmpeg::Packet>& packet);
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

} // namespace usb_cam
} // namespace nx