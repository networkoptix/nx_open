#include "decoder_registrar.h"

#include "video_decoder_registry.h"
#include "audio_decoder_registry.h"

#include "ffmpeg_video_decoder.h"
#include "ffmpeg_audio_decoder.h"

#include "proxy_video_decoder/proxy_video_decoder.h"
#include "android_video_decoder.h"
#include "android_audio_decoder.h"
#include "ios_video_decoder.h"

#include <nx/media/quick_sync/qsv_supported.h>

#ifdef __QSV_SUPPORTED__
#include "quick_sync/quick_sync_video_decoder.h"
#endif // __QSV_SUPPORTED__



#include "jpeg_decoder.h"

#include <nx/utils/log/assert.h>

namespace nx {
namespace media {

void DecoderRegistrar::registerDecoders(const Config config)
{
    VideoDecoderRegistry::instance()->setTranscodingEnabled(config.isTranscodingEnabled);

    // ATTENTION: Order of registration defines the priority of choosing: first comes first.

    #if defined(Q_OS_ANDROID)
    {
        static const int kHardwareDecodersCount = 1;
        VideoDecoderRegistry::instance()->addPlugin<AndroidVideoDecoder>(kHardwareDecodersCount);
        // HW audio decoder crashes in readOutputBuffer() for some reason. So far, disabling it.
        //AudioDecoderRegistry::instance()->addPlugin<AndroidAudioDecoder>();
    }
    #endif

    #if defined(Q_OS_IOS)
    if (config.enableHardwareDecoder)
    {
        static const int kHardwareDecodersCount = 1;
        VideoDecoderRegistry::instance()->addPlugin<IOSVideoDecoder>(kHardwareDecodersCount);
    }
    #endif

    #if defined(ENABLE_PROXY_DECODER)
    {
        static const int kHardwareDecodersCount = 1;
        VideoDecoderRegistry::instance()->addPlugin<ProxyVideoDecoder>(kHardwareDecodersCount);
    }
    #endif

    {
        FfmpegVideoDecoder::setMaxResolutions(config.maxFfmpegResolutions);

#if defined(__QSV_SUPPORTED__)
        if (config.enableHardwareDecoder)
            //VideoDecoderRegistry::instance()->addPlugin<quick_sync::QuickSyncVideoDecoder>();
#endif // __QSV_SUPPORTED__
        VideoDecoderRegistry::instance()->addPlugin<FfmpegVideoDecoder>();
        AudioDecoderRegistry::instance()->addPlugin<FfmpegAudioDecoder>();
    }

    VideoDecoderRegistry::instance()->addPlugin<JpegDecoder>();
}

} // namespace media
} // namespace nx
