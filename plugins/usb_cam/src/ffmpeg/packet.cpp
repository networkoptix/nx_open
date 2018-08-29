#include "packet.h"

#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
} // extern "C"

#ifdef _WIN32
#include <iterator>
#endif

namespace nx {
namespace ffmpeg {

namespace {

uint8_t firstDigit(uint8_t n)
{
    int result = 0;
    while (n % 10 != 0)
    {
        --n;
        ++result;
    }
    return result;
}

}

Packet::Packet(AVCodecID codecID, const std::shared_ptr<std::atomic_int>& packetCount):
    m_packet(av_packet_alloc()),
    m_codecID(codecID),
    m_packetCount(packetCount),
    m_timeStamp(0)
#ifdef _WIN32
    ,m_parseNalUnitsVisited(false)
#endif
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

AVCodecID Packet::codecID() const 
{
    return m_codecID; 
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
    /**
     * DirectShow implementation does not set /a m_packet->flags properly after a call to av_read_frame.
     */
#ifdef _WIN32
    // in the case the packet was produced by an encoder, this will be set apporpriately
    if (m_packet->flags & AV_PKT_FLAG_KEY)
        return true;
    parseNalUnits();
#endif
    /**
     * Video4Linux2 implementation sets it properly
     */
    return m_packet->flags & AV_PKT_FLAG_KEY;
}

#ifdef _WIN32
void Packet::parseNalUnits() const
{
    if (m_parseNalUnitsVisited)
        return;

    m_parseNalUnitsVisited = true;

    if (m_codecID != AV_CODEC_ID_H264)
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
} // namespace nx