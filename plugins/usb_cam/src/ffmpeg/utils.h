#pragma once

#include <string>

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/error.h>
} // extern "C"

#include <camera/camera_plugin_types.h>

namespace nx {
namespace ffmpeg {
namespace utils {

std::string errorToString(int errorCode);
std::string codecIDToName(AVCodecID codecID);
AVCodecID codecNameToID(const char * codecName);
AVPixelFormat suggestPixelFormat(AVCodecID codecID);
AVPixelFormat unDeprecatePixelFormat(AVPixelFormat pixelFormat);
nxcip::CompressionType toNxCompressionType(AVCodecID codecID);
AVCodecID toAVCodecID(nxcip::CompressionType);
void toFraction(float number, int *outNumerator, int * outDenominator);

} // namespace utils
} // namespace ffmpeg
} // namespace nx

