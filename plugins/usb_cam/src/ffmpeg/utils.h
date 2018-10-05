#pragma once

#include <string>

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/error.h>
} // extern "C"

#include <camera/camera_plugin.h>
#include <camera/camera_plugin_types.h>

namespace nx {
namespace usb_cam {
namespace ffmpeg {
namespace utils {

std::string errorToString(int errorCode);
std::string codecIDToName(AVCodecID codecId);
AVCodecID codecNameToID(const char * codecName);

AVPixelFormat suggestPixelFormat(const AVCodec* codec);
AVPixelFormat unDeprecatePixelFormat(AVPixelFormat pixelFormat);

nxcip::DataPacketType toNxDataPacketType(AVMediaType mediaType);
AVMediaType toAVMediaType(nxcip::DataPacketType mediaType);

nxcip::CompressionType toNxCompressionType(AVCodecID codecId);
AVCodecID toAVCodecID(nxcip::CompressionType compressionType);

nxcip::AudioFormat::SampleType toNxSampleType(
    AVSampleFormat format,
    bool * planar = nullptr,
    bool * ok = nullptr);

void toFraction(float number, int *outNumerator, int * outDenominator);

uint64_t suggestChannelLayout(const AVCodec* codec);
AVSampleFormat suggestSampleFormat(const AVCodec * codec);
int suggestSampleRate(const AVCodec * codec);

} // namespace utils
} // namespace ffmpeg
} // namespace usb_cam
} // namespace nx

