#include "seamless_video_decoder.h"

#include <deque>

#include <QtMultimedia/QVideoFrame>

#include <utils/media/jpeg_utils.h>
#include <utils/media/h264_utils.h>

#include "abstract_video_decoder.h"
#include "video_decoder_registry.h"
#include "frame_metadata.h"

namespace nx {
namespace media {

namespace {

QSize mediaSizeFromRawData(const QnConstCompressedVideoDataPtr& frame)
{
    const auto& context = frame->context;
    QSize result;
    switch (context->getCodecId())
    {
        case CODEC_ID_H264:
            extractSpsPps(frame, &result, nullptr);
            return result;
        case CODEC_ID_MJPEG:
        {
            nx_jpg::ImageInfo imgInfo;
            nx_jpg::readJpegImageInfo((const quint8*) frame->data(), frame->dataSize(), &imgInfo);
            return QSize(imgInfo.width, imgInfo.height);
        }
        default:
            return result;
    }
}

/**
 * This data is used to compare current and previous frame so as to reset video decoder if needed.
 */
struct FrameBasicInfo
{
    FrameBasicInfo()
    :
        codec(CODEC_ID_NONE)
    {
    }

    FrameBasicInfo(const QnConstCompressedVideoDataPtr& frame)
    :
        codec(CODEC_ID_NONE)
    {
        codec = frame->compressionType;
        size = QSize(frame->width, frame->height);
        if (size.isEmpty())
            size = mediaSizeFromRawData(frame);
    }

    QSize size;
    CodecID codec;
};

} // namespace

//-------------------------------------------------------------------------------------------------
// SeamlessVideoDecoderPrivate

class SeamlessVideoDecoderPrivate: public QObject
{
    Q_DECLARE_PUBLIC(SeamlessVideoDecoder)
    SeamlessVideoDecoder *q_ptr;

public:
    SeamlessVideoDecoderPrivate(SeamlessVideoDecoder *parent);

    void addMetadata(const QnConstCompressedVideoDataPtr& frame);
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

void SeamlessVideoDecoderPrivate::addMetadata(const QnConstCompressedVideoDataPtr& frame)
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

SeamlessVideoDecoder::SeamlessVideoDecoder()
:
    QObject(),
    d_ptr(new SeamlessVideoDecoderPrivate(this))
{
}

SeamlessVideoDecoder::~SeamlessVideoDecoder()
{
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
    bool isSimilarParams = frameInfo.codec == d->prevFrameInfo.codec;
    if (!frameInfo.size.isEmpty())
        isSimilarParams &= frameInfo.size == d->prevFrameInfo.size;
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
                const FrameMetadata metadata = d->findMetadata(
                    d->decoderFrameNumToLocalNum(decodedFrameNum));
                metadata.serialize(decodedFrame);
                d->queue.push_back(std::move(decodedFrame));
            }
        }

        // Release previous decoder in case the hardware decoder can handle only single instance.
        d->videoDecoder.reset();

        d->videoDecoder = VideoDecoderRegistry::instance()->createCompatibleDecoder(
            // TODO mike: Pass frame size to ...VideoDecoder.
            frame->compressionType, frameInfo.size);
        d->decoderFrameOffset = d->frameNumber;
        d->prevFrameInfo = frameInfo;
        d->clearMetadata();
    }

    d->addMetadata(frame);
    int decodedFrameNum = 0;
    if (d->videoDecoder)
    {
        QVideoFramePtr decodedFrame;
        decodedFrameNum = d->videoDecoder->decode(frame, &decodedFrame);
        if (decodedFrame)
        {
            const FrameMetadata metadata = d->findMetadata(
                d->decoderFrameNumToLocalNum(decodedFrameNum));
            metadata.serialize(decodedFrame);
            d->queue.push_back(std::move(decodedFrame));
        }
    }

    if (d->queue.empty())
        return decodedFrameNum >= 0; //< Return false if decoding fails and no queued frames are left.

    *result = std::move(d->queue.front());
    d->queue.pop_front();
    return true;
}

int SeamlessVideoDecoder::currentFrameNumber() const
{
    Q_D(const SeamlessVideoDecoder);
    return d->frameNumber;
}

} // namespace media
} // namespace nx
