#include "decoder_registrar.h"

#include "video_decoder_registry.h"
#include "audio_decoder_registry.h"

#if !defined(DISABLE_FFMPEG)
    #include "ffmpeg_video_decoder.h"
    #include "ffmpeg_audio_decoder.h"
#endif // !DISABLE_FFMPEG

#if defined(ENABLE_PROXY_DECODER)
    #include "proxy_video_decoder.h"
#endif // ENABLE_PROXY_DECODER

#if defined(Q_OS_ANDROID)
    #include "android_video_decoder.h"
    #include "android_audio_decoder.h"
#endif // Q_OS_ANDROID

#include "jpeg_decoder.h"

namespace nx {
namespace media {

void DecoderRegistrar::registerDecoders(
    std::shared_ptr<AbstractResourceAllocator> allocator,
    const QSize& maxFfmpegResolution,
    bool isTranscodingEnabled)
{
    assert(allocator);

    VideoDecoderRegistry::instance()->setTranscodingEnabled(isTranscodingEnabled);

    // ATTENTION: Order of registration defines the priority of choosing: first comes first.

    #if defined(Q_OS_ANDROID)
    {
        static const int kHardwareDecodersCount = 1;
        VideoDecoderRegistry::instance()->addPlugin<AndroidVideoDecoder>(allocator,
            kHardwareDecodersCount);
        AudioDecoderRegistry::instance()->addPlugin<AndroidAudioDecoder>();
    }
    #endif // Q_OS_ANDROID

    #if defined(ENABLE_PROXY_DECODER)
    {
        static const int kHardwareDecodersCount = 1;
        VideoDecoderRegistry::instance()->addPlugin<ProxyVideoDecoder>(allocator,
            kHardwareDecodersCount);
    }
    #endif // ENABLE_PROXY_DECODER

    #if !defined(DISABLE_FFMPEG)
    {
        FfmpegVideoDecoder::setMaxResolution(maxFfmpegResolution);

        VideoDecoderRegistry::instance()->addPlugin<FfmpegVideoDecoder>();
        AudioDecoderRegistry::instance()->addPlugin<FfmpegAudioDecoder>();
    }
    #endif // !DISABLE_FFMPEG

    VideoDecoderRegistry::instance()->addPlugin<JpegDecoder>();
}

} // namespace media
} // namespace nx
