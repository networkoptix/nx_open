// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "utils/common/byte_array.h"

#include "format.h"

namespace nx::audio {

class NX_VMS_COMMON_API Processor
{
public:
    static Format downmix(QnByteArray& audio, Format format);
    static Format float2int16(QnByteArray& audio, Format format);
    static Format float2int32(QnByteArray& audio, Format format);
    static Format int32Toint16(QnByteArray& audio, Format format);
};

} // namespace nx::audio
