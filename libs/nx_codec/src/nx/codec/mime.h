// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <string>

namespace nx::media {

constexpr std::string_view kMpjpegBoundary = "mjpeg_frame";

NX_CODEC_API std::string getMimeType(const AVCodecParameters* codecpar);
NX_CODEC_API std::string getFormatMimeType(const std::string_view& format);

} // namespace nx::media
