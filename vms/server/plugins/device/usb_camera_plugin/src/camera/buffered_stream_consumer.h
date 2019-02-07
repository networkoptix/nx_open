#pragma once

#include "abstract_stream_consumer.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace nx::usb_cam {

class BufferedPacketConsumer: public AbstractPacketConsumer
{
public:
    // AbstractPacketConsumer
    virtual void pushPacket(const std::shared_ptr<ffmpeg::Packet>& packet) override;
    virtual void flush() override;

public:
    BufferedPacketConsumer(const std::string& cameraId): m_cameraId(cameraId) {}
    std::shared_ptr<ffmpeg::Packet> pop(); //< will blocked if no packets available
    void interrupt();

private:
    void push(const std::shared_ptr<ffmpeg::Packet>& packet);

private:
    std::atomic_bool m_dropUntilNextVideoKeyPacket = true;
    std::atomic_int m_bufferSizeBytes = 0;
    std::condition_variable m_wait;
    mutable std::mutex m_mutex;
    std::deque<std::shared_ptr<ffmpeg::Packet>> m_buffer;
    bool m_interrupted = false;
    std::string m_cameraId;
};

} // namespace nx::usb_cam
