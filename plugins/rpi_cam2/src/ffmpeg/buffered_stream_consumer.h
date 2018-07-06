#pragma once

#include "abstract_stream_consumer.h"

#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace ffmpeg {

class BufferedStreamConsumer : public AbstractStreamConsumer
{
public:
    BufferedStreamConsumer(
        const std::weak_ptr<StreamReader>& streamReader,
        const CodecParameters& params);
    
    virtual void givePacket(const std::shared_ptr<Packet>& packet) override;
    std::shared_ptr<Packet> popNextPacket();

private:
    nx::utils::SyncQueue<std::shared_ptr<Packet>> m_packets;
};

} //namespace ffmpeg
} //namespace nx