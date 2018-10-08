#include "packet.h"

#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
} // extern "C"

#ifdef _WIN32
#include <iterator>
#endif

namespace nx {
namespace usb_cam {
namespace ffmpeg {

namespace {

uint8_t firstDigit(uint8_t n)
{
    int result = 0;
    while (n-- % 10 != 0)
        ++result;
    return result;
}

}

Packet::Packet(
    AVCodecID codecId,
    AVMediaType mediaType,
    const std::shared_ptr<std::atomic_int>& packetCount)
    :
    m_codecId(codecId),
    m_mediaType(mediaType),
    m_packetCount(packetCount),
    m_packet(av_packet_alloc())
{
    if (m_packetCount)
        ++(*m_packetCount);
    initialize();
}

Packet::~Packet()
{
    if (m_packetCount)
        --(*m_packetCount);
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
    // DirectShow implementation does not set AVPacket::flags properly after a call to av_read_frame.
#ifdef _WIN32
    // in the case the packet was produced by an encoder, this will be set apporpriately
    if (m_packet->flags & AV_PKT_FLAG_KEY)
        return true;

    parseNalUnits();
#endif
    
    // Video4Linux2 implementation sets it properly
    return m_packet->flags & AV_PKT_FLAG_KEY;
}

#ifdef _WIN32
void Packet::parseNalUnits() const
{
    if (m_parseNalUnitsVisited)
        return;

    m_parseNalUnitsVisited = true;

    if (m_codecId != AV_CODEC_ID_H264)
        return;

    const uint8_t* buffer = data();
    const uint8_t* bufStart = buffer;
    const uint8_t* end = buffer + size();

    while (buffer < end)
    {
        if (*buffer > 1)
            buffer += 3;
        else if (*buffer == 0)
            buffer++;
        else if (buffer[-2] == 0 && buffer[-1] == 0)
        {
            if (buffer - 3 >= bufStart && buffer[-3] == 0)
            {
                if (buffer + 1 < end)
                {
                    uint8_t n = firstDigit(buffer[1]);
                    if (n % 5 == 0)
                    {
                        m_packet->flags |= AV_PKT_FLAG_KEY;
                        break;
                    }
                }
            }
            buffer += 3;
        }
        else
            buffer += 3;
    }
}
#endif

} // namespace ffmpeg
} // namespace usb_cam
} // namespace nx