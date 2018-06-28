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
    ~Packet();

    AVPacket * packet() const;

    void initialize();
    void unreference();
    
    int copy(Packet * outPacket) const;

private:
    AVPacket* m_packet;
};

} // namespace ffmpeg
} // namespace nx
