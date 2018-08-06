#include "packet.h"

extern "C"{
#include <libavcodec/avcodec.h>
} // extern "C"

namespace nx {
namespace ffmpeg {

Packet::Packet(AVCodecID codecID, const std::shared_ptr<std::atomic_int>& packetCount) :
    m_packet(av_packet_alloc()),
    m_codecID(codecID),
    m_timeStamp(0)
{
    initialize();
}

Packet::~Packet()
{
    av_packet_free(&m_packet);
}

int Packet::size() const
{
    return m_packet->size;
}

uint8_t * Packet::data() const
{
    return m_packet->data;
}

int Packet::flags() const
{
    return m_packet->flags;
}

int64_t Packet::pts() const
{
    return m_packet->pts;
}

AVPacket * Packet::packet() const
{
    return m_packet;
}

void Packet::initialize()
{
    av_init_packet(m_packet);
    m_packet->data = nullptr;
    m_packet->size = 0;
}

void Packet::unreference()
{
    av_packet_unref(m_packet);
}

AVCodecID Packet::codecID() const
{
    return m_codecID;
}

int Packet::copy(Packet * outPacket) const
{
    outPacket->m_timeStamp = m_timeStamp;
    outPacket->m_codecID = m_codecID;
    int copyCode = av_copy_packet(outPacket->m_packet, m_packet);
    if(copyCode < 0)
        return copyCode;
    return av_packet_copy_props(outPacket->m_packet, m_packet);


}

uint64_t Packet::timeStamp() const
{
    return m_timeStamp;
}

void Packet::setTimeStamp(uint64_t millis)
{
    m_timeStamp = millis;
}

bool Packet::keyPacket() const
{
    return m_packet->flags & AV_PKT_FLAG_KEY;
}

} // namespace ffmpeg
} // namespace nx