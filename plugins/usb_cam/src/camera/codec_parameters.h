#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <string>

#include "ffmpeg/utils.h"

namespace nx {
namespace usb_cam {

struct CodecParameters
{
    AVCodecID codecID;
    float fps;
    int bitrate;
    int width;
    int height;

    CodecParameters():
        codecID(AV_CODEC_ID_NONE),
        fps(0),
        bitrate(0),
        width(0),
        height(0)
    {
    }

    CodecParameters(AVCodecID codecID, float fps, int bitrate, int width, int height):
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

    std::string toString() const
    {
        return std::string("codec: ") + ffmpeg::utils::codecIDToName(codecID) + 
            ", res:" + std::to_string(width) + "x" + std::to_string(height) + 
            ", fps: " + std::to_string(fps) + 
            ", bitrate: " +  std::to_string(bitrate);
    }
};

} //namespace usb_cam
} //namespace nx