#pragma once

extern "C" {
#include <libavdevice/avdevice.h>
} // extern "C"

#include <camera/camera_plugin_types.h>

namespace nx {
namespace webcam_plugin {
namespace utils {
namespace av {

AVStream* getAVStream(AVFormatContext * context, int * streamIndex, AVMediaType mediaType);
AVPixelFormat suggestPixelFormat(AVCodecID codecID);
AVPixelFormat unDeprecatePixelFormat(AVPixelFormat pixelFormat);
nxcip::CompressionType toNxCompressionType(AVCodecID codecID);
AVCodecID toAVCodecID(nxcip::CompressionType);

} // namespace av
} // namespace utils
} // namespace webcam_plugin
} // namespace nx

