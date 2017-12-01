#include "seamless_video_decoder.h"

#include <deque>

#include <QtMultimedia/QVideoFrame>
#include <QtCore/QMutexLocker>

#include "abstract_video_decoder.h"
#include "video_decoder_registry.h"
#include "frame_metadata.h"
#include <utils/media/utils.h>

namespace nx {
namespace media {

namespace {

/**
 * This data is used to compare current and previous frame so as to reset video decoder if needed.
 */
struct FrameBasicInfo
{
    FrameBasicInfo()
    :
        codec(AV_CODEC_ID_NONE)
    {
    }

    FrameBasicInfo(const QnConstCompressedVideoDataPtr& frame)
    :
        codec(AV_CODEC_ID_NONE)
    {
        codec = frame->compressionType;
        size = QSize(frame->width, frame->height);
        if (size.isEmpty())
            size = nx::media::AbstractVideoDecoder::mediaSizeFromRawData(frame);
    }

    QSize size;
    AVCodecID codec;
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
    SeamlessVideoDecoderPrivate(SeamlessVideoDecoder *parent);

    // Store metadata from compressed frame.
    void pushMetadata(const QnConstCompressedVideoDataPtr& frame);

    // Restore metadata for decoded frame.
    void popMetadata(const QVideoFramePtr& frame, int decodedFrameNum);

    const FrameMetadata findMetadata(int frameNum);
    void clearMetadata();
    int decoderFrameNumToLocalNum(int value) const;
public:
    std::deque<QVideoFramePtr> queue; /**< Temporary  buffer for decoded data. */
    VideoDecoderPtr videoDecoder;
    FrameBasicInfo prevFrameInfo;
    int frameNumber; /**< Absolute frame number. */

    /** Relative frame number (frameNumber value when the decoder was created). */
    int decoderFrameOffset;

    /** Associate extra information with output frames which corresponds to input frames. */
    std::deque<FrameMetadata> metadataQueue;

    bool allowOverlay;
    SeamlessVideoDecoder::VideoGeometryAccessor videoGeometryAccessor;
};

SeamlessVideoDecoderPrivate::SeamlessVideoDecoderPrivate(SeamlessVideoDecoder *parent)
:
    QObject(parent),
    q_ptr(parent),
    videoDecoder(nullptr, nullptr),
    frameNumber(0),
    decoderFrameOffset(0)
{
}

void SeamlessVideoDecoderPrivate::pushMetadata(const QnConstCompressedVideoDataPtr& frame)
{
    FrameMetadata metadata(frame);
    metadata.frameNum = frameNumber++;
    metadataQueue.push_back(std::move(metadata));
}

void SeamlessVideoDecoderPrivate::popMetadata(const QVideoFramePtr& frame, int decodedFrameNum)
{
    FrameMetadata metadata = findMetadata(decoderFrameNumToLocalNum(decodedFrameNum));
    const double sar = videoDecoder->getSampleAspectRatio();
    // some decoders doesn't fill sar in spite of it isn't 1.0
    metadata.sar = qFuzzyCompare(sar, 1.0)
        ? nx::media::getDefaultSampleAspectRatio(frame->size())
        : sar;
    if (videoDecoder->capabilities().testFlag(AbstractVideoDecoder::Capability::hardwareAccelerated))
        metadata.flags |= QnAbstractMediaData::MediaFlags_HWDecodingUsed;
    metadata.serialize(frame);
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

SeamlessVideoDecoder::SeamlessVideoDecoder():
    QObject(),
    d_ptr(new SeamlessVideoDecoderPrivate(this))
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


void SeamlessVideoDecoder::setVideoGeometryAccessor(VideoGeometryAccessor videoGeometryAccessor)
{
    Q_D(SeamlessVideoDecoder);
    d->videoGeometryAccessor = videoGeometryAccessor;
}

void SeamlessVideoDecoder::pleaseStop()
{
}

bool SeamlessVideoDecoder::decode(
    const QnConstCompressedVideoDataPtr& frame, QVideoFramePtr* result)
{
    Q_D(SeamlessVideoDecoder);
    if (result)
        result->reset();

    FrameBasicInfo frameInfo(frame);
    bool isSimilarParams;
    {
        QMutexLocker lock(&mutex);
        isSimilarParams = frameInfo.codec == d->prevFrameInfo.codec;
        if (!frameInfo.size.isEmpty())
            isSimilarParams &= frameInfo.size == d->prevFrameInfo.size;
    }
    if (!isSimilarParams)
    {
        if (d->videoDecoder)
        {
            for (;;)
            {
                QVideoFramePtr decodedFrame;
                int decodedFrameNum = d->videoDecoder->decode(
                    QnConstCompressedVideoDataPtr(), &decodedFrame);
                if (!decodedFrame)
                    break; //< decoder's buffer is flushed
                d->popMetadata(decodedFrame, decodedFrameNum);
                d->queue.push_back(std::move(decodedFrame));
            }
        }

        // Release previous decoder in case the hardware decoder can handle only single instance.
        d->videoDecoder.reset();

        d->videoDecoder = VideoDecoderRegistry::instance()->createCompatibleDecoder(
            frame->compressionType, frameInfo.size, d->allowOverlay);
        if (d->videoDecoder)
            d->videoDecoder->setVideoGeometryAccessor(d->videoGeometryAccessor);
        d->decoderFrameOffset = d->frameNumber;
        {
            QMutexLocker lock(&mutex);
            d->prevFrameInfo = frameInfo;
        }
        d->clearMetadata();
    }

    d->pushMetadata(frame);
    int decodedFrameNum = 0;
    if (d->videoDecoder)
    {
        QVideoFramePtr decodedFrame;
        decodedFrameNum = d->videoDecoder->decode(frame, &decodedFrame);
        if (decodedFrame)
        {
            d->popMetadata(decodedFrame, decodedFrameNum);
            d->queue.push_back(std::move(decodedFrame));
        }
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
