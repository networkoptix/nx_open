// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "decoder_registrar.h"

#include <nx/utils/log/assert.h>

#include "audio_decoder_registry.h"
#include "video_decoder_registry.h"

#include "ffmpeg_audio_decoder.h"
#include "ffmpeg_video_decoder.h"
#include "ffmpeg_hw_video_decoder.h"

#include "jpeg_decoder.h"

namespace nx::media {

void DecoderRegistrar::registerDecoders(const QMap<int, QSize>& maxFfmpegResolutions,
    int maxHardwareDecodersCount)
{
    // ATTENTION: Order of registration defines the priority of choosing: first comes first.

    VideoDecoderRegistry::instance()->addPlugin<FfmpegHwVideoDecoder>(
        "FfmpegHwVideoDecoder", maxHardwareDecodersCount);

    {
        FfmpegVideoDecoder::setMaxResolutions(maxFfmpegResolutions);

        VideoDecoderRegistry::instance()->addPlugin<FfmpegVideoDecoder>("FfmpegVideoDecoder");
        AudioDecoderRegistry::instance()->addPlugin<FfmpegAudioDecoder>();
    }

    VideoDecoderRegistry::instance()->addPlugin<JpegDecoder>("JpegDecoder");
}

} // namespace nx::media
