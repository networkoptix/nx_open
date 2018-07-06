#pragma once

extern "C"{
#include <libavcodec/avcodec.h>
} // extern "C"

#include "error.h"

class AVPacket;

namespace nx {
namespace ffmpeg {

class Packet
{
public:
    Packet();
    Packet(const AVCodecID & codecID);
    ~Packet();

    AVPacket * packet() const;

    void initialize();
    void unreference();
    
    AVCodecID codecID() const;
    void setCodecID(const AVCodecID& codecID);

    int copy(Packet * outPacket) const;

private:
    AVCodecID m_codecID;
    AVPacket* m_packet;
};

} // namespace ffmpeg
} // namespace nx
