#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <string>

#include "utils.h"

namespace nx {
namespace ffmpeg {

struct CodecParameters
{
    AVCodecID codecID;
    int fps;
    int bitrate;
    int width;
    int height;

    CodecParameters(AVCodecID codecID):
        codecID(codecID),
        fps(0),
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

    std::string toString() const
    {
        std::string resolution = std::to_string(width) + "x" + std::to_string(height);
        return std::string("codec: ") + utils::avCodecIDStr(codecID) + ", res:"
            + resolution + ", fps: " + std::to_string(fps) + ", bitrate: " +  std::to_string(bitrate);
    }
};

} //namespace ffmpeg
} //namespace nx