#include <deque>

#include "seamless_audio_decoder.h"
#include "abstract_audio_decoder.h"
#include "audio_decoder_registry.h"

namespace nx {
namespace media {

namespace {

struct FrameBasicInfo
{
    FrameBasicInfo()
    :
        codec(CODEC_ID_NONE)
    {
    }

    FrameBasicInfo(const QnConstCompressedAudioDataPtr& frame)
    :
        codec(CODEC_ID_NONE)
    {
        codec = frame->compressionType;
    }

    CodecID codec;
};

} // namespace

//-------------------------------------------------------------------------------------------------
// SeamlessAudioDecoderPrivate

class SeamlessAudioDecoderPrivate: public QObject
{
    Q_DECLARE_PUBLIC(SeamlessAudioDecoder)
    SeamlessAudioDecoder *q_ptr;

public:
    SeamlessAudioDecoderPrivate(SeamlessAudioDecoder *parent);

public:
    AudioDecoderPtr audioDecoder;
    FrameBasicInfo prevFrameInfo;
};

SeamlessAudioDecoderPrivate::SeamlessAudioDecoderPrivate(SeamlessAudioDecoder* parent)
:
    QObject(parent),
    q_ptr(parent)
{
}

//-------------------------------------------------------------------------------------------------
// SeamlessAudioDecoder

SeamlessAudioDecoder::SeamlessAudioDecoder()
:
    QObject(),
    d_ptr(new SeamlessAudioDecoderPrivate(this))
{
}

SeamlessAudioDecoder::~SeamlessAudioDecoder()
{
}

void SeamlessAudioDecoder::pleaseStop()
{
}

bool SeamlessAudioDecoder::decode(
    const QnConstCompressedAudioDataPtr& frame, QnAudioFramePtr* result)
{
    Q_D(SeamlessAudioDecoder);
    if (result)
        result->clear();

    FrameBasicInfo frameInfo(frame);
    bool isSimilarParams = frameInfo.codec == d->prevFrameInfo.codec;
    if (!isSimilarParams)
    {
        // Release previous decoder in case the hardware decoder can handle only single instance.
        d->audioDecoder.reset();

        d->audioDecoder = AudioDecoderRegistry::instance()->createCompatibleDecoder(
            frame->compressionType);
        d->prevFrameInfo = frameInfo;
    }
        
    if (!d->audioDecoder)
        return false;
    QnAudioFramePtr decodedFrame = d->audioDecoder->decode(frame);
    if (!decodedFrame)
        return false;

    *result = std::move(decodedFrame);
    return !result->isNull();
}

} // namespace media
} // namespace nx
