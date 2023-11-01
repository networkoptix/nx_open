// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "seamless_video_decoder.h"

#include <deque>

#include <QtCore/QMutexLocker>

#include <nx/media/h264_utils.h>
#include <nx/media/utils.h>
#include <nx/media/video_frame.h>
#include <nx/utils/log/log.h>

#include "abstract_video_decoder.h"
#include "frame_metadata.h"
#include "video_decoder_registry.h"

namespace nx {
namespace media {

namespace {

/**
 * This data is used to compare current and previous frame so as to reset video decoder if needed.
 */
struct FrameBasicInfo
{
    FrameBasicInfo() = default;
    FrameBasicInfo(const QnConstCompressedVideoDataPtr& frame)
    {
        codec = frame->compressionType;
        size = getFrameSize(frame.get());
    }

    QSize size;
    AVCodecID codec = AV_CODEC_ID_NONE;
};

static QMutex mutex;

} // namespace

//-------------------------------------------------------------------------------------------------
// SeamlessVideoDecoderPrivate

class SeamlessVideoDecoderPrivate: public QObject
{
    Q_DECLARE_PUBLIC(SeamlessVideoDecoder)
    SeamlessVideoDecoder *q_ptr;

public:
    SeamlessVideoDecoderPrivate(
        SeamlessVideoDecoder* parent, RenderContextSynchronizerPtr renderContextSynchronizer);

    // Store metadata from compressed frame.
    void pushMetadata(const QnConstCompressedVideoDataPtr& frame);


    const FrameMetadata findMetadata(int frameNum);
    void clearMetadata();
    int decoderFrameNumToLocalNum(int value) const;

    void updateSar(const QnConstCompressedVideoDataPtr& frame);

public:
    std::deque<VideoFramePtr> queue; /**< Temporary  buffer for decoded data. */
    VideoDecoderPtr videoDecoder;
    CodecParametersConstPtr currentCodecParameters;
    FrameBasicInfo prevFrameInfo;
    int frameNumber; /**< Absolute frame number. */

    /** Relative frame number (frameNumber value when the decoder was created). */
    int decoderFrameOffset;

    double sar = 1.0;

    /** Associate extra information with output frames which corresponds to input frames. */
    std::deque<FrameMetadata> metadataQueue;

    bool allowOverlay;
    bool allowHardwareAcceleration = false;
    bool resetDecoder = false;
    SeamlessVideoDecoder::VideoGeometryAccessor videoGeometryAccessor;

    RenderContextSynchronizerPtr renderContextSynchronizer;
};

SeamlessVideoDecoderPrivate::SeamlessVideoDecoderPrivate(
    SeamlessVideoDecoder* parent,
    RenderContextSynchronizerPtr renderContextSynchronizer)
    :
    QObject(parent),
    q_ptr(parent),
    videoDecoder(nullptr, nullptr),
    frameNumber(0),
    decoderFrameOffset(0),
    renderContextSynchronizer(renderContextSynchronizer)
{
}

void SeamlessVideoDecoderPrivate::updateSar(const QnConstCompressedVideoDataPtr& frame)
{
    switch (frame->compressionType)
    {
        case AV_CODEC_ID_H264:
        {
            SPSUnit sps;
            if (!nx::media::h264::extractSps(frame.get(), sps))
                return;
            sar = sps.getSar();
            break;
        }
        case AV_CODEC_ID_H263:
        case AV_CODEC_ID_MJPEG:
            sar = 1.0;
            break;
        default:
            NX_VERBOSE(this, "SAR parser is not implemented for codec %1", frame->compressionType);
    }
}

void SeamlessVideoDecoderPrivate::pushMetadata(const QnConstCompressedVideoDataPtr& frame)
{
    FrameMetadata metadata(frame);
    metadata.frameNum = frameNumber++;
    metadataQueue.push_back(std::move(metadata));
}

const FrameMetadata SeamlessVideoDecoderPrivate::findMetadata(int frameNum)
{
    while (!metadataQueue.empty() && metadataQueue.front().frameNum < frameNum)
        metadataQueue.pop_front(); //< In case decoder skipped some input frames.

    FrameMetadata result;
    if (!metadataQueue.empty() && metadataQueue.front().frameNum == frameNum)
    {
        result = (std::move(metadataQueue.front()));
        metadataQueue.pop_front();
    }
    return result;
}

void SeamlessVideoDecoderPrivate::clearMetadata()
{
    metadataQueue.clear();
}

int SeamlessVideoDecoderPrivate::decoderFrameNumToLocalNum(int value) const
{
    return value + decoderFrameOffset;
}

//-------------------------------------------------------------------------------------------------
// SeamlessVideoDecoder

SeamlessVideoDecoder::SeamlessVideoDecoder(RenderContextSynchronizerPtr renderContextSynchronizer):
    QObject(),
    d_ptr(new SeamlessVideoDecoderPrivate(this, renderContextSynchronizer))
{
}

SeamlessVideoDecoder::~SeamlessVideoDecoder()
{
}

void SeamlessVideoDecoder::setAllowOverlay(bool value)
{
    Q_D(SeamlessVideoDecoder);
    d->allowOverlay = value;
}

void SeamlessVideoDecoder::setAllowHardwareAcceleration(bool value)
{
    Q_D(SeamlessVideoDecoder);
    if (d->allowHardwareAcceleration == value)
        return;

    d->allowHardwareAcceleration = value;
    d->resetDecoder = true;
}

void SeamlessVideoDecoder::setVideoGeometryAccessor(VideoGeometryAccessor videoGeometryAccessor)
{
    Q_D(SeamlessVideoDecoder);
    d->videoGeometryAccessor = videoGeometryAccessor;
}

void SeamlessVideoDecoder::pleaseStop()
{
}

bool SeamlessVideoDecoder::decode(
    const QnConstCompressedVideoDataPtr& frame, VideoFramePtr* result)
{
    Q_D(SeamlessVideoDecoder);
    if (result)
        result->reset();

    d->updateSar(frame);
    FrameBasicInfo frameInfo(frame);
    bool isSimilarParams;
    {
        QMutexLocker lock(&mutex);
        isSimilarParams = frameInfo.codec == d->prevFrameInfo.codec;
        if (!frameInfo.size.isEmpty())
            isSimilarParams &= frameInfo.size == d->prevFrameInfo.size;

        if (d->currentCodecParameters && frame->context)
            isSimilarParams &= frame->context->isEqual(*d->currentCodecParameters);
    }
    if (!isSimilarParams || (d->resetDecoder && frame->flags & QnAbstractMediaData::MediaFlags_AVKey))
    {
        d->resetDecoder = false;
        if (d->videoDecoder)
        {
            for (;;)
            {
                VideoFramePtr decodedFrame;
                int decodedFrameNum = d->videoDecoder->decode(
                    QnConstCompressedVideoDataPtr(), &decodedFrame);
                if (!decodedFrame)
                    break; //< decoder's buffer is flushed

                pushFrame(decodedFrame, decodedFrameNum, d->sar);
            }
        }

        // Release previous decoder in case the hardware decoder can handle only single instance.
        d->videoDecoder.reset();
        d->videoDecoder = VideoDecoderRegistry::instance()->createCompatibleDecoder(
            frame->compressionType,
            frameInfo.size,
            d->allowOverlay,
            d->allowHardwareAcceleration,
            d->renderContextSynchronizer);
        if (d->videoDecoder)
            d->videoDecoder->setVideoGeometryAccessor(d->videoGeometryAccessor);
        d->decoderFrameOffset = d->frameNumber;
        d->sar = 1.0;

        {
            QMutexLocker lock(&mutex);
            d->prevFrameInfo = frameInfo;
            d->currentCodecParameters = frame->context;
        }
        d->clearMetadata();
    }

    d->pushMetadata(frame);
    int decodedFrameNum = 0;
    if (d->videoDecoder)
    {
        VideoFramePtr decodedFrame;
        decodedFrameNum = d->videoDecoder->decode(frame, &decodedFrame);
        if (decodedFrame)
            pushFrame(decodedFrame, decodedFrameNum, d->sar);
    }

    if (d->queue.empty())
    {
        // Return false if decoding fails and no queued frames are left.
        return decodedFrameNum >= 0;
    }

    *result = std::move(d->queue.front());
    d->queue.pop_front();
    return true;
}

void SeamlessVideoDecoder::pushFrame(VideoFramePtr decodedFrame, int decodedFrameNum, double sar)
{
    Q_D(SeamlessVideoDecoder);
    FrameMetadata metadata = d->findMetadata(d->decoderFrameNumToLocalNum(decodedFrameNum));
    metadata.sar = sar;
    if (qFuzzyCompare(metadata.sar, 1.0))
        metadata.sar = nx::media::getDefaultSampleAspectRatio(decodedFrame->size());

    if (d->videoDecoder->capabilities().testFlag(AbstractVideoDecoder::Capability::hardwareAccelerated))
        metadata.flags |= QnAbstractMediaData::MediaFlags_HWDecodingUsed;

    metadata.serialize(decodedFrame);
    d->queue.push_back(std::move(decodedFrame));
}

int SeamlessVideoDecoder::currentFrameNumber() const
{
    Q_D(const SeamlessVideoDecoder);
    return d->frameNumber;
}

QSize SeamlessVideoDecoder::currentResolution() const
{
    Q_D(const SeamlessVideoDecoder);
    QMutexLocker lock(&mutex);
    return d->prevFrameInfo.size;
}

AVCodecID SeamlessVideoDecoder::currentCodec() const
{
    Q_D(const SeamlessVideoDecoder);
    QMutexLocker lock(&mutex);
    return d->prevFrameInfo.codec;
}

AbstractVideoDecoder* SeamlessVideoDecoder::currentDecoder() const
{
    Q_D(const SeamlessVideoDecoder);
    QMutexLocker lock(&mutex);
    return d->videoDecoder.get();
}

} // namespace media
} // namespace nx
