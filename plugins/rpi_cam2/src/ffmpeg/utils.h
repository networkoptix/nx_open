#pragma once

#include <string>

extern "C" {
#include <libavdevice/avdevice.h>
} // extern "C"

#include <camera/camera_plugin_types.h>

namespace nx {
namespace ffmpeg {
namespace utils {

std::string avCodecIDStr(AVCodecID codecID);
AVCodecID avCodecID(const char * codecName);
AVStream* getAVStream(AVFormatContext * context, AVMediaType mediaType, int * streamIndex = nullptr);
AVPixelFormat suggestPixelFormat(AVCodecID codecID);
AVPixelFormat unDeprecatePixelFormat(AVPixelFormat pixelFormat);
nxcip::CompressionType toNxCompressionType(AVCodecID codecID);
AVCodecID toAVCodecID(nxcip::CompressionType);

} // namespace utils
} // namespace ffmpeg
} // namespace nx

