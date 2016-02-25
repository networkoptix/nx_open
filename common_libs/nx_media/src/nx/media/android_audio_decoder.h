#pragma once

#include <QtCore/QObject>

#if defined(Q_OS_ANDROID)

#include <nx/streaming/audio_data_packet.h>

#include "abstract_audio_decoder.h"

namespace nx {
namespace media {

/*
* This class implements hardware android video decoder
*/
class AndroidAudioDecoderPrivate;
class AndroidAudioDecoder : public AbstractAudioDecoder
{
public:
    AndroidAudioDecoder();
    virtual ~AndroidAudioDecoder();

    static bool isCompatible(const CodecID codec);
    virtual AudioFramePtr decode(const QnConstCompressedAudioDataPtr& frame) override;
private:
    QScopedPointer<AndroidAudioDecoderPrivate> d_ptr;
    Q_DECLARE_PRIVATE(AndroidAudioDecoder);
};

}
}

#endif // #defined(Q_OS_ANDROID)
