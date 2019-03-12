#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>

#include "ffmpeg/packet.h"

namespace nx::usb_cam {

class PacketBuffer
{
public:
    PacketBuffer(const std::string& cameraId): m_cameraId(cameraId) {}

    void pushPacket(const std::shared_ptr<ffmpeg::Packet>& packet);
    std::shared_ptr<ffmpeg::Packet> pop(); //< will blocked if no packets available
    void interrupt();
    void flush();

private:
    void push(const std::shared_ptr<ffmpeg::Packet>& packet);

private:
    std::atomic_bool m_dropUntilNextVideoKeyPacket = true;
    std::atomic_int m_bufferSizeBytes = 0;
    std::condition_variable m_wait;
    std::mutex m_mutex;
    std::deque<std::shared_ptr<ffmpeg::Packet>> m_buffer;
    bool m_interrupted = false;
    std::string m_cameraId;
};

} // namespace nx::usb_cam
