#pragma once

#include "abstract_stream_consumer.h"

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

    virtual int size() override;
    virtual void givePacket(const std::shared_ptr<Packet>& packet) override;
    std::shared_ptr<Packet> popFront();
    std::shared_ptr<Packet> peekFront() const;
    std::shared_ptr<Packet> peekBack() const;
    void clear();

    int dropOldNonKeyPackets();

private:
    mutable std::mutex m_mutex;
    std::deque<std::shared_ptr<Packet>> m_vector;
    bool m_ignoreNonKeyPackets;
};

} //namespace ffmpeg
} //namespace nx