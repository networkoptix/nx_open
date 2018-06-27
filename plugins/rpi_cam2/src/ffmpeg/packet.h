#pragma once

extern "C"{
#include <libavcodec/avcodec.h>
} // extern "C"

#include "error.h"

namespace nx {
namespace webcam_plugin {
namespace ffmpeg {

class Packet
{
public:
    Packet():
        m_packet(av_packet_alloc())
    {
        if(m_packet)
            initialize();
        else
            error::setLastError(AVERROR(ENOMEM));
    }

    ~Packet()
    {
        av_packet_free(&m_packet);
    }

    AVPacket * packet() const
    {
        return m_packet;
    }

    void initialize()
    {
        av_init_packet(m_packet);
        m_packet->data = nullptr;
        m_packet->size = 0;
    }

    void unreference()
    {
        av_packet_unref(m_packet);
    }

    int copy(Packet * outPacket) const
    {
        int copyCode = av_copy_packet(outPacket->m_packet, m_packet);
        error::updateIfError(copyCode);
        return copyCode;
    }

private:
    AVPacket* m_packet;
};

} // namespace ffmpeg
} // namespace webcam_plugin
} // namespace nx
