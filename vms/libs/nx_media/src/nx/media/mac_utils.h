// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSize>

extern "C" {
#include <libavcodec/codec_id.h>
} // extern "C"

QSize mac_maxDecodeResolutionH264();
QSize mac_maxDecodeResolutionHevc();
bool mac_isHWDecodingSupported(const AVCodecID codec);
