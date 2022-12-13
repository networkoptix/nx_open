// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_audio_decoder.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
} // extern "C"

#include <nx/media/ffmpeg_audio_filter.h>
#include <nx/media/ini.h>
#include <nx/utils/log/log.h>
#include <utils/media/ffmpeg_helper.h>

namespace nx {
namespace media {

//-------------------------------------------------------------------------------------------------
// FfmpegAudioDecoderPrivate

class FfmpegAudioDecoderPrivate: public QObject
{
    Q_DECLARE_PUBLIC(FfmpegAudioDecoder)
    FfmpegAudioDecoder* q_ptr;

public:
    FfmpegAudioDecoderPrivate():
        frame(av_frame_alloc())
    {
    }

    ~FfmpegAudioDecoderPrivate()
    {
        avcodec_free_context(&codecContext);
        av_frame_free(&frame);
    }

    void initContext(const QnConstCompressedAudioDataPtr& frame, double speed);

    AVFrame* frame = nullptr;
    AVCodecContext* codecContext = nullptr;
    CodecParametersConstPtr abstractContext;
    FfmpegAudioFilter filter;
    qint64 lastPts = AV_NOPTS_VALUE;
    bool gotFrame = false;
    bool enableFilter = false;
    std::unique_ptr<QnFfmpegAudioHelper> audioHelper;
};

void FfmpegAudioDecoderPrivate::initContext(
    const QnConstCompressedAudioDataPtr& frame, double speed)
{
    if (!frame)
        return;

    auto codec = avcodec_find_decoder(frame->compressionType);
    codecContext = avcodec_alloc_context3(codec);
    if (frame->context)
    {
        frame->context->toAvCodecContext(codecContext);
        abstractContext = frame->context;
    }
    if (avcodec_open2(codecContext, codec, nullptr) < 0)
    {
        qWarning() << "Can't open decoder for codec" << frame->compressionType;
        avcodec_free_context(&codecContext);
    }

    if (ini().allowSpeedupAudio && speed != 1.0)
    {
        char filterDescr[256];
        snprintf(filterDescr, sizeof(filterDescr), "atempo='%lf'", speed);
        if (!filter.init(codecContext, {1,1000000}, filterDescr))
            NX_WARNING(this, "Can't init audio filter");
    }
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

bool FfmpegAudioDecoder::isCompatible(const AVCodecID /*codec*/, double /*speed*/)
{
    return true;
}

bool FfmpegAudioDecoder::decode(const QnConstCompressedAudioDataPtr& frame, double speed)
{
    Q_D(FfmpegAudioDecoder);

    if (!d->codecContext)
    {
        d->initContext(frame, speed);
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
            memset(avpkt.data + avpkt.size, 0, AV_INPUT_BUFFER_PADDING_SIZE);

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

    d->gotFrame = true;
    d->enableFilter = fabs(speed - 1.0) >= kSpeedStep && ini().allowSpeedupAudio;
    if (d->enableFilter)
    {
        if (!d->filter.pushFrame(d->frame))
            return false;
    }
    return true;
}

AudioFramePtr FfmpegAudioDecoder::nextFrame()
{
    Q_D(FfmpegAudioDecoder);

    AVFrame* outputFrame = nullptr;
    if (d->enableFilter)
    {
        outputFrame = d->filter.nextFrame();
        if (!outputFrame)
            return nullptr;
    }
    else if (d->gotFrame)
    {
        outputFrame = d->frame;
        d->gotFrame = false;
    }
    else
    {
        return nullptr;
    }
    int frameSize = av_samples_get_buffer_size(
        0, //< output. plane size, optional
        d->codecContext->channels,
        outputFrame->nb_samples,
        d->codecContext->sample_fmt,
        1); //< buffer size alignment. 1 - no alignment (exact size)

    AudioFramePtr resultFrame = std::make_shared<AudioFrame>();
    if (!d->audioHelper)
        d->audioHelper.reset(new QnFfmpegAudioHelper(d->codecContext));

    resultFrame->data.resize(frameSize);
    d->audioHelper->copyAudioSamples((quint8*) resultFrame->data.data(), outputFrame);

    resultFrame->context = d->abstractContext;

    // Ffmpeg pts/dts are mixed up here, so it's pkt_dts. Also Convert usec to msec.
    resultFrame->timestampUsec = outputFrame->pkt_dts;
    return resultFrame;
}

} // namespace media
} // namespace nx
