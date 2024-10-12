// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "av_options.h"

extern "C" {
#include <libavutil/dict.h>
} // extern "C"

#include <nx/utils/log/log.h>

namespace nx::media::ffmpeg {

AvOptions::~AvOptions()
{
    if (m_options)
        av_dict_free(&m_options);
}

void AvOptions::set(const char* key, const char* value, int flags)
{
    if (av_dict_set(&m_options, key, value, flags) < 0)
        NX_WARNING(this, "Failed to configure ffmpeg object, key: %1, value: %2", key, value);
}

void AvOptions::set(const char* key, int64_t value, int flags)
{
    if (av_dict_set_int(&m_options, key, value, flags) < 0)
        NX_WARNING(this, "Failed to configure ffmpeg object, key: %1, value: %2", key, value);
}

} // namespace nx::media::ffmpeg
