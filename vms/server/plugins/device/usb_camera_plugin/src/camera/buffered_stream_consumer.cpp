#include "buffered_stream_consumer.h"

#include <nx/utils/log/log.h>

#include "ffmpeg/packet.h"
#include "ffmpeg/frame.h"

namespace nx::usb_cam {

namespace {

static constexpr int kBufferMaxSize(1024*1024*8);

}

void BufferedPacketConsumer::givePacket(const std::shared_ptr<ffmpeg::Packet>& packet)
{
    if (m_bufferSizeBytes > kBufferMaxSize)
    {
        NX_WARNING(this, "USB camera plugin buffer overflow!");
        m_dropUntilNextVideoKeyPacket = true;
        return;
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
    m_buffer.clear();
}

} // namespace nx::usb_cam
