// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "decoder_registrar.h"

#include <nx/media/supported_decoders.h>
#include <nx/utils/log/assert.h>

#include "audio_decoder_registry.h"
#include "video_decoder_registry.h"

#include "ffmpeg_audio_decoder.h"
#include "ffmpeg_video_decoder.h"

#include "android_audio_decoder.h"
#include "android_video_decoder.h"
#include "jpeg_decoder.h"
#include "mac_video_decoder.h"

#if NX_MEDIA_QUICK_SYNC_DECODER_SUPPORTED
    #include "quick_sync/quick_sync_video_decoder.h"
#endif

namespace nx::media {

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

        #if NX_MEDIA_QUICK_SYNC_DECODER_SUPPORTED
            //VideoDecoderRegistry::instance()->addPlugin<quick_sync::QuickSyncVideoDecoder>(
            //    "QuickSyncVideoDecoder");
        #endif

        VideoDecoderRegistry::instance()->addPlugin<FfmpegVideoDecoder>("FfmpegVideoDecoder");
        AudioDecoderRegistry::instance()->addPlugin<FfmpegAudioDecoder>();
    }

    VideoDecoderRegistry::instance()->addPlugin<JpegDecoder>("JpegDecoder");
}

} // namespace nx::media
