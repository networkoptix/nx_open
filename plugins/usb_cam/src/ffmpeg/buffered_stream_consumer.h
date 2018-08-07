#pragma once

#include "abstract_stream_consumer.h"

#include <condition_variable>
#include <mutex>
#include <deque>

namespace nx {
namespace ffmpeg {

class BufferedPacketConsumer : public AbstractPacketConsumer
{
public:
    BufferedPacketConsumer(
        const std::weak_ptr<StreamReader>& streamReader,
        const CodecParameters& params);

    virtual void givePacket(const std::shared_ptr<Packet>& packet) override;
    std::shared_ptr<Packet> popFront();
    int size() const;
    void clear() override;

    int dropOldNonKeyPackets();
    void interrupt();

    void dropUntilFirstKeyPacket();

private:
    std::condition_variable m_wait;
    mutable std::mutex m_mutex;
    std::deque<std::shared_ptr<Packet>> m_packets;
    bool m_ignoreNonKeyPackets;
    bool m_interrupted;
};


////////////////////////////////////// BufferedFrameConsumer ///////////////////////////////////////


class BufferedFrameConsumer : public AbstractFrameConsumer
{
public:
    BufferedFrameConsumer(
        const std::weak_ptr<StreamReader>& streamReader,
        const CodecParameters& params);

    virtual void giveFrame(const std::shared_ptr<Frame>& frame) override;
    std::shared_ptr<Frame> popFront();
    int size() const;
    void clear() override;

    void interrupt();

private:
    std::condition_variable m_wait;
    mutable std::mutex m_mutex;
    std::deque<std::shared_ptr<Frame>> m_frames;
    bool m_interrupted;
};

} //namespace ffmpeg
} //namespace nx