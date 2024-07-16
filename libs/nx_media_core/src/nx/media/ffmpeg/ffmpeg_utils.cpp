// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_utils.h"

#include <string>

extern "C" {
#include <libavutil/error.h>
} // extern "C"

namespace nx::media::ffmpeg {

std::string avErrorToString(int errorCode)
{
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errorCode, errorBuffer, AV_ERROR_MAX_STRING_SIZE);
    return errorBuffer;
}

} // namespace nx::media::ffmpeg
