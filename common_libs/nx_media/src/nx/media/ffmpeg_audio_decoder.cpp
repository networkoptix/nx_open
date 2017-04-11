#include "ffmpeg_audio_decoder.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
} // extern "C"

#include <utils/media/ffmpeg_helper.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace media {

//-------------------------------------------------------------------------------------------------
// FfmpegAudioDecoderPrivate

class FfmpegAudioDecoderPrivate: public QObject
{
    Q_DECLARE_PUBLIC(FfmpegAudioDecoder)
    FfmpegAudioDecoder* q_ptr;

public:
    FfmpegAudioDecoderPrivate()
    :
        frame(av_frame_alloc()),
        codecContext(nullptr),
        lastPts(AV_NOPTS_VALUE)
    {
    }

    ~FfmpegAudioDecoderPrivate()
    {
        closeCodecContext();
        av_frame_free(&frame);
    }

    void initContext(const QnConstCompressedAudioDataPtr& frame);
    void closeCodecContext();

    AVFrame* frame;
    AVCodecContext* codecContext;
    QnConstMediaContextPtr abstractContext;
    qint64 lastPts;
    std::unique_ptr<QnFfmpegAudioHelper> audioHelper;
};

void FfmpegAudioDecoderPrivate::initContext(const QnConstCompressedAudioDataPtr& frame)
{
    if (!frame)
        return;

    auto codec = avcodec_find_decoder(frame->compressionType);
    codecContext = avcodec_alloc_context3(codec);
    if (frame->context)
    {
        QnFfmpegHelper::mediaContextToAvCodecContext(codecContext, frame->context);
        abstractContext = frame->context;
    }
    if (avcodec_open2(codecContext, codec, nullptr) < 0)
    {
        qWarning() << "Can't open decoder for codec" << frame->compressionType;
        closeCodecContext();
    }
}

void FfmpegAudioDecoderPrivate::closeCodecContext()
{
    QnFfmpegHelper::deleteAvCodecContext(codecContext);
    codecContext = 0;
}

//-------------------------------------------------------------------------------------------------
// FfmpegAudioDecoder

FfmpegAudioDecoder::FfmpegAudioDecoder()
:
    AbstractAudioDecoder(),
    d_ptr(new FfmpegAudioDecoderPrivate())
{
}

FfmpegAudioDecoder::~FfmpegAudioDecoder()
{
}

bool FfmpegAudioDecoder::isCompatible(const AVCodecID codec)
{
    Q_UNUSED(codec);
    return true;
}

bool FfmpegAudioDecoder::decode(const QnConstCompressedAudioDataPtr& frame, AudioFramePtr* const outFrame)
{
    Q_D(FfmpegAudioDecoder);

    if (!d->codecContext)
    {
        d->initContext(frame);
        if (!d->codecContext)
            return false;
    }

    QnFfmpegAvPacket avpkt;
    if (frame)
    {
        avpkt.data = (unsigned char*)frame->data();
        avpkt.size = static_cast<int>(frame->dataSize());
        avpkt.dts = avpkt.pts = frame->timestamp;
        if (frame->flags & QnAbstractMediaData::MediaFlags_AVKey)
            avpkt.flags = AV_PKT_FLAG_KEY;

        // It's already guaranteed by QnByteArray that there is an extra space reserved. We mus
        // fill the padding bytes according to ffmpeg documentation.
        if (avpkt.data)
            memset(avpkt.data + avpkt.size, 0, FF_INPUT_BUFFER_PADDING_SIZE);

        d->lastPts = frame->timestamp;
    }
    else
    {
        // There is a known ffmpeg bug. It returns the below time for the very last packet while
        // flushing the internal buffer. So, repeat this time for the empty packet in order to
        // avoid the bug.
        avpkt.pts = avpkt.dts = d->lastPts;
    }

    int gotData = 0;
    avcodec_decode_audio4(d->codecContext, d->frame, &gotData, &avpkt);

    if (gotData <= 0)
        return false; //< Negative value means error.

    int frameSize = av_samples_get_buffer_size(
        0, //< output. plane size, optional
        d->codecContext->channels,
        d->frame->nb_samples,
        d->codecContext->sample_fmt,
        1); //< buffer size alignment. 1 - no alignment (exact size)

    nx::AudioFrame* audioFrame = new nx::AudioFrame();

    if (!d->audioHelper)
        d->audioHelper.reset(new QnFfmpegAudioHelper(d->codecContext));
    audioFrame->data.resize(frameSize);
    d->audioHelper->copyAudioSamples((quint8*) audioFrame->data.data(), d->frame);

    audioFrame->context = d->abstractContext;

    // Ffmpeg pts/dts are mixed up here, so it's pkt_dts. Also Convert usec to msec.
    audioFrame->timestampUsec = d->frame->pkt_dts;

    outFrame->reset(audioFrame);
    return true;
}

} // namespace media
} // namespace nx
