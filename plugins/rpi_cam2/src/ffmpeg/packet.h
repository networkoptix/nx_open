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
    Packet()
    {
        init();
    }

    ~Packet()
    {
        unref();
    }

    AVPacket * ffmpegPacket()
    {
        return &m_packet;
    }

    void unref()
    {
        av_packet_unref(&m_packet);
    }

    void init()
    {
        av_init_packet(&m_packet);
        m_packet.data = nullptr;
        m_packet.size = 0;
    }

private:
    AVPacket m_packet;
};

} // namespace ffmpeg
} // namespace webcam_plugin
} // namespace nx