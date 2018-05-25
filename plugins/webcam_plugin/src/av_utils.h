#pragma once

#include <string>

extern "C" {
#include <libavdevice/avdevice.h>
} // extern "C"

#include <camera/camera_plugin_types.h>

namespace nx {
namespace webcam_plugin {
namespace utils {
namespace av {

std::string avStrError(int errorCode);
AVStream* getAVStream(AVFormatContext * context, AVMediaType mediaType, int * streamIndex = nullptr);
AVPixelFormat suggestPixelFormat(AVCodecID codecID);
AVPixelFormat unDeprecatePixelFormat(AVPixelFormat pixelFormat);
nxcip::CompressionType toNxCompressionType(AVCodecID codecID);
AVCodecID toAVCodecID(nxcip::CompressionType);

} // namespace av
} // namespace utils
} // namespace webcam_plugin
} // namespace nx

