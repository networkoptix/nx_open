#pragma once

#include <memory>
#include <vector>

#include "abstract_stream_consumer.h"
#include "abstract_video_consumer.h"
#include "ffmpeg/frame.h"
#include "ffmpeg/packet.h"

namespace nx {
namespace usb_cam {

//--------------------------------------------------------------------------------------------------
// StreamConsumerManager

class StreamConsumerManager
{
public:
    bool empty() const;
    size_t size() const;
    /**
     * Calls flush() on each AbstractStreamConsumer
     */
    void flush();

    /**
     * Add a consumer to the consumer manager.
     * 
     * @param[in] consumer - the consumer to add.
     * @return - index the consumer was added at or -1 if it was already added.
     */
    virtual size_t addConsumer(const std::weak_ptr<AbstractStreamConsumer>& consumer);

    /**
     * Remove the givenconsumer from the consumer manager.
     *
     * @param[in] consumer - the consumer to remove.
     * @return - the index the consumer was add before it was removed or -1 if it couldn't be found.
     */
    virtual size_t removeConsumer(const std::weak_ptr<AbstractStreamConsumer>& consumer);
    int consumerIndex(const std::weak_ptr<AbstractStreamConsumer>& consumer) const;
    const std::vector<std::weak_ptr<AbstractStreamConsumer>>& consumers() const;

    std::weak_ptr<AbstractVideoConsumer> largestBitrate(int * outBitrate = nullptr) const;
    std::weak_ptr<AbstractVideoConsumer> largestFps(float * outFps = nullptr) const;
    std::weak_ptr<AbstractVideoConsumer> largestResolution(int * outWidth = nullptr, int * outHeight = nullptr) const;

protected:
    std::vector<std::weak_ptr<AbstractStreamConsumer>> m_consumers;
};

//--------------------------------------------------------------------------------------------------
// FrameConsumerManager

class FrameConsumerManager: public StreamConsumerManager
{
public:
    void giveFrame(const std::shared_ptr<ffmpeg::Frame>& frame);
};

//--------------------------------------------------------------------------------------------------
// PacketConsumerManager

class PacketConsumerManager: public StreamConsumerManager
{
public:
    virtual size_t addConsumer(const std::weak_ptr<AbstractStreamConsumer>& consumer) override;
    virtual size_t removeConsumer(const std::weak_ptr<AbstractStreamConsumer>& consumer) override;
    size_t addConsumer(const std::weak_ptr<AbstractStreamConsumer>& consumer, bool waitForKeyPacket);
    
    void givePacket(const std::shared_ptr<ffmpeg::Packet>& packet);
    
private:
    /** 
     * Indexes here correspond to indexes in the parent class' vector, m_consumers.
     * If true, the consumer at a given index will not receive a packet until it is a key packet,
     * and then sets the flag to false.
     * If false, the consumer receives all packets.
     */
    std::vector<bool> m_waitForKeyPacket;
};

} // namespace usb_cam
} // namespace nx