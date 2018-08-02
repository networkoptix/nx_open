#pragma once

#include "abstract_stream_consumer.h"

#include <condition_variable>
#include <mutex>
#include <deque>

namespace nx {
namespace ffmpeg {

class BufferedStreamConsumer : public AbstractStreamConsumer
{
public:
    BufferedStreamConsumer(
        const std::weak_ptr<StreamReader>& streamReader,
        const CodecParameters& params);

    virtual void givePacket(const std::shared_ptr<Packet>& packet) override;
    std::shared_ptr<Packet> popFront();
    int size();
    void clear() override;

    int dropOldNonKeyPackets();
    void interrupt();

    void ignoreNonKeyPackets();

private:
    std::condition_variable m_wait;
    mutable std::mutex m_mutex;
    std::deque<std::shared_ptr<Packet>> m_packets;
    bool m_ignoreNonKeyPackets;
    bool m_interrupted;
};

} //namespace ffmpeg
} //namespace nx