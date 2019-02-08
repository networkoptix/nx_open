#pragma once
#if defined(Q_OS_ANDROID)

#include <QtCore/QObject>

#include <nx/streaming/audio_data_packet.h>

#include "abstract_audio_decoder.h"

namespace nx {
namespace media {

class AndroidAudioDecoderPrivate;

/**
 * Implements hardware android audio decoder.
 */
class AndroidAudioDecoder
:
    public AbstractAudioDecoder
{
public:
    AndroidAudioDecoder();
    virtual ~AndroidAudioDecoder();

    static bool isCompatible(const AVCodecID codec);
    virtual bool decode(
        const QnConstCompressedAudioDataPtr& frame, AudioFramePtr* const outFrame) override;
private:
    static bool isDecoderCompatibleToPlatform();
private:
    QScopedPointer<AndroidAudioDecoderPrivate> d_ptr;
    Q_DECLARE_PRIVATE(AndroidAudioDecoder);
};

} // namespace media
} // namespace nx

#endif // Q_OS_ANDROID
