#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <string>

namespace nx {
namespace ffmpeg {

struct CodecParameters
    {
        AVCodecID codecID = AV_CODEC_ID_NONE;
        int fps = 0;
        int bitrate = 0;
        int width = 0;
        int height = 0;

        CodecParameters(AVCodecID codecID, int fps, int bitrate, int width, int height):
            codecID(codecID),
            fps(fps),
            bitrate(bitrate),
            width(width),
            height(height)
        {
        }

        void setResolution(int width, int height)
        {
            this->width = width;
            this->height = height;
        }

        void resolution(int * width, int * height) const
        {
            *width = this->width;
            *height = this->height;
        }
    };

} //namespace ffmpeg
} //namespace nx