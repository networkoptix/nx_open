// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/media/audio/format.h>
#include <nx/utils/byte_array.h>

namespace nx::media::audio {

class NX_MEDIA_CORE_API Processor
{
public:
    static Format downmix(nx::utils::ByteArray& audio, Format format);
    static Format float2int16(nx::utils::ByteArray& audio, Format format);
    static Format float2int32(nx::utils::ByteArray& audio, Format format);
    static Format int32Toint16(nx::utils::ByteArray& audio, Format format);
};

} // namespace nx::media::audio
