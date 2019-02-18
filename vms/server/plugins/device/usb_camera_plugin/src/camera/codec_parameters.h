#pragma once

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
}
#include <camera/camera_plugin.h>

#include "ffmpeg/utils.h"

namespace nx::usb_cam {

struct CodecParameters
{
    AVCodecID codecId = AV_CODEC_ID_NONE;
    float fps = 0;
    int bitrate = 0;
    nxcip::Resolution resolution;

    CodecParameters() = default;

    CodecParameters(AVCodecID codecId, float fps, int bitrate, const nxcip::Resolution& resolution):
        codecId(codecId),
        fps(fps),
        bitrate(bitrate),
        resolution(resolution)
    {
    }

    std::string toString() const
    {
        return std::string("codec: ") + ffmpeg::utils::codecIdToName(codecId) +
            ", res:" + std::to_string(resolution.width) + "x" + std::to_string(resolution.height) +
            ", fps: " + std::to_string(fps) +
            ", bitrate: " +  std::to_string(bitrate);
    }
};

} // namespace usb_cam::nx
