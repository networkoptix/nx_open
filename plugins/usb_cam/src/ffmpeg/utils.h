#pragma once

#include <string>

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/error.h>
} // extern "C"

#include <camera/camera_plugin.h>
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
nxcip::AudioFormat::SampleType toNxSampleType(
    AVSampleFormat format,
    bool * planar = nullptr,
    bool * ok = nullptr);
AVCodecID toAVCodecID(nxcip::CompressionType compressionType);
void toFraction(float number, int *outNumerator, int * outDenominator);

} // namespace utils
} // namespace ffmpeg
} // namespace nx

