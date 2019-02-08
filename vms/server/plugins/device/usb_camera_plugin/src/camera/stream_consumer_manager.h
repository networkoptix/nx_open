#pragma once

#include <memory>
#include <vector>

#include "abstract_stream_consumer.h"
#include "abstract_video_consumer.h"
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
     * @return - the index the consumer was add before it was removed or -1 if not found.
     */
    virtual size_t removeConsumer(const std::weak_ptr<AbstractStreamConsumer>& consumer);
    int consumerIndex(const std::weak_ptr<AbstractStreamConsumer>& consumer) const;

    int findLargestBitrate() const;
    float findLargestFps() const;
    nxcip::Resolution findLargestResolution() const;

protected:
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
    enum class ConsumerState
    {
        skipUntilNextKeyPacket,
        receiveEveryPacket,
    };

public:
    virtual size_t addConsumer(const std::weak_ptr<AbstractStreamConsumer>& consumer) override;
    virtual size_t removeConsumer(const std::weak_ptr<AbstractStreamConsumer>& consumer) override;
    void givePacket(const std::shared_ptr<ffmpeg::Packet>& packet);

    size_t addConsumer(
        const std::weak_ptr<AbstractStreamConsumer>& consumer,
        const ConsumerState& waitForKeyPacket);

private:
    // Indexes here correspond to indexes AbstractStreamConsumerManager.m_consumers
    std::vector<ConsumerState> m_consumerKeyPacketStates;
};

} // namespace usb_cam
} // namespace nx
