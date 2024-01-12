// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "decoder_registrar.h"

#include "video_decoder_registry.h"
#include "audio_decoder_registry.h"

#include "ffmpeg_video_decoder.h"
#include "ffmpeg_audio_decoder.h"

#include "android_video_decoder.h"
#include "android_audio_decoder.h"
#include "mac_video_decoder.h"

#include <nx/media/quick_sync/qsv_supported.h>
#ifdef __QSV_SUPPORTED__
#include "quick_sync/quick_sync_video_decoder.h"
#endif // __QSV_SUPPORTED__



#include "jpeg_decoder.h"

#include <nx/utils/log/assert.h>

namespace nx {
namespace media {

void DecoderRegistrar::registerDecoders(const QMap<int, QSize>& maxFfmpegResolutions)
{
    // ATTENTION: Order of registration defines the priority of choosing: first comes first.

    #if defined(Q_OS_ANDROID)
    {
        static const int kHardwareDecodersCount = 1;
        VideoDecoderRegistry::instance()->addPlugin<AndroidVideoDecoder>(
            "AndroidVideoDecoder", kHardwareDecodersCount);
        // HW audio decoder crashes in readOutputBuffer() for some reason. So far, disabling it.
        //AudioDecoderRegistry::instance()->addPlugin<AndroidAudioDecoder>();
    }
    #endif

    #if defined(Q_OS_IOS) || defined(Q_OS_MACOS)
    {
        static const int kHardwareDecodersCount = 1;
        VideoDecoderRegistry::instance()->addPlugin<MacVideoDecoder>(
            "MacVideoDecoder", kHardwareDecodersCount);
    }
    #endif

    #if defined(ENABLE_PROXY_DECODER)
    {
        static const int kHardwareDecodersCount = 1;
        VideoDecoderRegistry::instance()->addPlugin<ProxyVideoDecoder>(
            "ProxyVideoDecoder", kHardwareDecodersCount);
    }
    #endif

    {
        FfmpegVideoDecoder::setMaxResolutions(maxFfmpegResolutions);

#if defined(__QSV_SUPPORTED__)
        //VideoDecoderRegistry::instance()->addPlugin<quick_sync::QuickSyncVideoDecoder>(
        //    "QuickSyncVideoDecoder");
#endif // __QSV_SUPPORTED__
        VideoDecoderRegistry::instance()->addPlugin<FfmpegVideoDecoder>("FfmpegVideoDecoder");
        AudioDecoderRegistry::instance()->addPlugin<FfmpegAudioDecoder>();
    }

    VideoDecoderRegistry::instance()->addPlugin<JpegDecoder>("JpegDecoder");
}

} // namespace media
} // namespace nx
