#include "packet.h"

extern "C"{
#include <libavcodec/avcodec.h>
} // extern "C"

#include "error.h"

namespace nx {
namespace ffmpeg {

Packet::Packet():
    m_packet(av_packet_alloc())
{
    if(m_packet)
        initialize();
    else
        error::setLastError(AVERROR(ENOMEM));
}

Packet::~Packet()
{
    av_packet_free(&m_packet);
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

int Packet::copy(Packet * outPacket) const
{
    int copyCode = av_copy_packet(outPacket->m_packet, m_packet);
    error::updateIfError(copyCode);
    return copyCode;
}

} // namespace ffmpeg
} // namespace nx