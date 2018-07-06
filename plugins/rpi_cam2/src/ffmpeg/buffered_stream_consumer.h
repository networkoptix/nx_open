#pragma once

#include "stream_reader.h"

#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace ffmpeg {

class BufferedStreamConsumer : public StreamConsumer
{
public:
    BufferedStreamConsumer(
        const CodecParameters& params, 
        const std::weak_ptr<StreamReader>& streamReader);
    
    void resize(int size);
    void setFps(int fps);
    void setResolution(int width, int height);
    void setBitrate(int bitrate);

    virtual int fps() const override;
    virtual void resolution(int * width, int * height) const override;
    virtual int bitrate() const override;
    virtual void givePacket(const std::shared_ptr<Packet>& packet) override;
    std::shared_ptr<Packet> popNextPacket();

private:
    CodecParameters m_params;
    nx::utils::SyncQueue<std::shared_ptr<Packet>> m_packets;
    int m_queueSize;
};

} //namespace ffmpeg
} //namespace nx