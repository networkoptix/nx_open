#include "decoder_registrar.h"

#include "video_decoder_registry.h"
#include "audio_decoder_registry.h"

#include "ffmpeg_video_decoder.h"
#include "ffmpeg_audio_decoder.h"

#include "proxy_video_decoder/proxy_video_decoder.h"
#include "android_video_decoder.h"
#include "android_audio_decoder.h"
#include "ios_video_decoder.h"

#include "jpeg_decoder.h"

#include <nx/utils/log/assert.h>

namespace nx {
namespace media {

void DecoderRegistrar::registerDecoders(
    std::shared_ptr<AbstractResourceAllocator> allocator,
    const QMap<AVCodecID, QSize>& maxFfmpegResolutions,
    bool isTranscodingEnabled)
{
    NX_ASSERT(allocator);

    VideoDecoderRegistry::instance()->setTranscodingEnabled(isTranscodingEnabled);

    // ATTENTION: Order of registration defines the priority of choosing: first comes first.

    #if defined(Q_OS_ANDROID)
    {
        static const int kHardwareDecodersCount = 1;
        VideoDecoderRegistry::instance()->addPlugin<AndroidVideoDecoder>(allocator,
            kHardwareDecodersCount);
        // HW audio decoder crashes in readOutputBuffer() for some reason. So far, disabling it.
        //AudioDecoderRegistry::instance()->addPlugin<AndroidAudioDecoder>();
    }
    #endif

    #if defined(Q_OS_IOS)
    {
        static const int kHardwareDecodersCount = 1;
        VideoDecoderRegistry::instance()->addPlugin<IOSVideoDecoder>(allocator,
            kHardwareDecodersCount);
    }
    #endif

    #if defined(ENABLE_PROXY_DECODER)
    {
        static const int kHardwareDecodersCount = 1;
        VideoDecoderRegistry::instance()->addPlugin<ProxyVideoDecoder>(allocator,
            kHardwareDecodersCount);
    }
    #endif

    {
        FfmpegVideoDecoder::setMaxResolutions(maxFfmpegResolutions);

        VideoDecoderRegistry::instance()->addPlugin<FfmpegVideoDecoder>();
        AudioDecoderRegistry::instance()->addPlugin<FfmpegAudioDecoder>();
    }

    VideoDecoderRegistry::instance()->addPlugin<JpegDecoder>();
}

} // namespace media
} // namespace nx
