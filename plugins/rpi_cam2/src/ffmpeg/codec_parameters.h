#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace nx {
namespace ffmpeg {

struct CodecParameters
    {
        AVCodecID codecID;
        int fps;
        int bitrate;
        int width;
        int height;

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