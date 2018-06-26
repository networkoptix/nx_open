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
        av_init_packet(&m_packet);
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

private:
    AVPacket m_packet;
};

} // namespace ffmpeg
} // namespace webcam_plugin
} // namespace nx