#include "decoder_registrar.h"

#include "video_decoder_registry.h"
#include "audio_decoder_registry.h"

#include "ffmpeg_video_decoder.h"
#include "ffmpeg_audio_decoder.h"

#include "proxy_video_decoder/proxy_video_decoder.h"
#include "android_video_decoder.h"
#include "android_audio_decoder.h"
#include "ios_video_decoder.h"
#include "quick_sync/quick_sync_video_decoder.h"

#include "jpeg_decoder.h"

#include <nx/utils/log/assert.h>

namespace nx {
namespace media {

void DecoderRegistrar::registerDecoders(
    const QMap<int, QSize>& maxFfmpegResolutions,
    bool isTranscodingEnabled,
    bool enableHardwareDecoderOnIPhone)
{
    VideoDecoderRegistry::instance()->setTranscodingEnabled(isTranscodingEnabled);

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
    if (enableHardwareDecoderOnIPhone)
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
        FfmpegVideoDecoder::setMaxResolutions(maxFfmpegResolutions);

        VideoDecoderRegistry::instance()->addPlugin<QuickSyncVideoDecoder>();
        VideoDecoderRegistry::instance()->addPlugin<FfmpegVideoDecoder>();
        AudioDecoderRegistry::instance()->addPlugin<FfmpegAudioDecoder>();
    }

    VideoDecoderRegistry::instance()->addPlugin<JpegDecoder>();
}

} // namespace media
} // namespace nx
