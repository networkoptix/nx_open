// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <string>

namespace nx::media {

NX_CODEC_API std::string getMimeType(const AVCodecParameters* codecpar);

} // namespace nx::media
