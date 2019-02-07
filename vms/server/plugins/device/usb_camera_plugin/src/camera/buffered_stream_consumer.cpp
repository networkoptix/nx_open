#include "buffered_stream_consumer.h"

#include <nx/utils/log/log.h>

#include "ffmpeg/packet.h"
#include "ffmpeg/frame.h"

namespace nx::usb_cam {

namespace {

static constexpr int kBufferMaxSize(1024 * 1024 * 8); // 8 MB.

}

void BufferedPacketConsumer::pushPacket(const std::shared_ptr<ffmpeg::Packet>& packet)
{
    if (m_bufferSizeBytes > kBufferMaxSize)
    {
        NX_WARNING(this, "USB camera %1 error: buffer overflow!", m_cameraId);
        flush();
    }

    if (packet->mediaType() == AVMEDIA_TYPE_VIDEO && m_dropUntilNextVideoKeyPacket)
    {
        if (!packet->keyPacket())
            return;
        m_dropUntilNextVideoKeyPacket = false;
    }
    push(packet);
}

void BufferedPacketConsumer::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dropUntilNextVideoKeyPacket = true;
    m_bufferSizeBytes = 0;
    m_buffer.clear();
}

void BufferedPacketConsumer::push(const std::shared_ptr<ffmpeg::Packet>& packet)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_buffer.emplace_back(packet);
    m_bufferSizeBytes += packet->size();
    m_wait.notify_all();
}

std::shared_ptr<ffmpeg::Packet> BufferedPacketConsumer::pop()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_wait.wait(lock, [this]() { return m_interrupted || !m_buffer.empty(); });
    if (m_buffer.empty())
        return nullptr;

    std::shared_ptr<ffmpeg::Packet> packet = m_buffer.front();
    m_buffer.pop_front();
    m_bufferSizeBytes -= packet->size();
    return packet;
}

void BufferedPacketConsumer::interrupt()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_interrupted = true;
    m_wait.notify_all();
}

} // namespace nx::usb_cam
