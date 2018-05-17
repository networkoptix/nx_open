#pragma once

#include <camera/camera_plugin_types.h>
extern "C" {
#include <libavdevice/avdevice.h>
}

namespace nx {
namespace utils{
namespace av{

    AVStream* getAvStream(AVFormatContext * context, int * streamIndex, enum AVMediaType mediaType);
    AVPixelFormat suggestPixelFormat(AVCodecID codecID);
    AVPixelFormat unDeprecatePixelFormat(AVPixelFormat pixelFormat);
    nxcip::CompressionType toNxCompressionType(AVCodecID codecID);

} // namespace av
} // namespace utils
} // namespace nx

