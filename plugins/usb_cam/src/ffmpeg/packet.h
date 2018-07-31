#pragma once

extern "C"{
#include <libavcodec/avcodec.h>
} // extern "C"

struct AVPacket;

namespace nx {
namespace ffmpeg {

class Packet
{
public:
    Packet(AVCodecID codecID);
    ~Packet();

    int size() const;
    uint8_t * data() const;
    int flags() const;
    int64_t pts() const;
    AVPacket * packet() const;

    void setBuffer(uint8_t * data, int size, bool avMalloced);

    void initialize();
    void unreference();
    
    AVCodecID codecID() const;

    int copy(Packet * outPacket) const;

    uint64_t timeStamp() const;
    void setTimeStamp(uint64_t millis);

    bool keyPacket() const;

private:
    AVPacket* m_packet;
    AVCodecID m_codecID;
    uint64_t m_timeStamp;
};

} // namespace ffmpeg
} // namespace nx
