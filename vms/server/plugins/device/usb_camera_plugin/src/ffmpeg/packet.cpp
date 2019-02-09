#include "packet.h"

namespace nx {
namespace usb_cam {
namespace ffmpeg {

Packet::Packet(AVCodecID codecId, AVMediaType mediaType)
    :
    m_codecId(codecId),
    m_mediaType(mediaType),
    m_packet(av_packet_alloc())
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

uint8_t* Packet::data()
{
    return m_packet->data;
}

const uint8_t* Packet::data() const
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

int64_t Packet::dts() const
{
    return m_packet->dts;
}

AVPacket* Packet::packet() const
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

int Packet::newPacket(int size)
{
    return av_new_packet(m_packet, size);
}

AVCodecID Packet::codecId() const
{
    return m_codecId;
}

AVMediaType Packet::mediaType() const
{
    return m_mediaType;
}

uint64_t Packet::timestamp() const
{
    return m_timestamp;
}

void Packet::setTimestamp(uint64_t millis)
{
    m_timestamp = millis;
}

bool Packet::keyPacket() const
{
    return m_packet->flags & AV_PKT_FLAG_KEY;
}

int Packet::copy(AVPacket& source)
{
    *m_packet = source;
    m_packet->side_data = NULL;
    m_packet->side_data_elems = 0;
    m_packet->data = (uint8_t*)av_malloc(source.size);
    if (!m_packet->data)
        return AVERROR(ENOMEM);

    memcpy(m_packet->data, source.data, source.size);
    m_packet->buf = av_buffer_create(m_packet->data, m_packet->size, av_buffer_default_free, NULL, 0);
    if (!m_packet->buf)
    {
        av_freep(&(m_packet->data));
        return AVERROR(ENOMEM);
    }
    return 0;
}

} // namespace ffmpeg
} // namespace usb_cam
} // namespace nx
