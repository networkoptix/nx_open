#include <deque>

#include <QCache>

#include "seamless_video_decoder.h"
#include "abstract_video_decoder.h"
#include "video_decoder_registry.h"
#include "frame_metadata.h"

#include <utils/media/jpeg_utils.h>
#include <utils/media/h264_utils.h>

namespace
{
    static const int kMetadataMaxSize = 16;
    

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
                nx_jpg::readJpegImageInfo((const quint8*)frame->data(), frame->dataSize(), &imgInfo);
                return QSize(imgInfo.width, imgInfo.height);
            }
            default:
                return result;
        }
    }

	/** This data is used to compare current and previous frame so as to reset video decoder if needed */
	struct FrameBasicInfo
	{
		FrameBasicInfo(): 
			codec(CODEC_ID_NONE)
		{

		}
		
		FrameBasicInfo(const QnConstCompressedVideoDataPtr& frame):
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
}

namespace nx {
namespace media {


// ----------------------------------- SeamlessVideoDecoderPrivate -----------------------------------

class SeamlessVideoDecoderPrivate : public QObject
{
	Q_DECLARE_PUBLIC(SeamlessVideoDecoder)
	SeamlessVideoDecoder *q_ptr;
public:
	SeamlessVideoDecoderPrivate(SeamlessVideoDecoder *parent);

	void addMetadata(const QnConstCompressedVideoDataPtr& frame);
	const FrameMetadata* findMetadata(int frameNum) const;
	void clearMetadata();
    int decoderFrameNumToLocalNum(int value) const;
public:
    std::deque<QnVideoFramePtr> queue; //< temporary  buffer for decoded data
	VideoDecoderPtr videoDecoder;
	FrameBasicInfo prevFrameInfo;
	int frameNumber;         //< absolute frame number
    int decoderFrameOffset;  //< relate frame number (frameNumber value when decoder was created)
	QCache<int, FrameMetadata> metadataCache;
};

SeamlessVideoDecoderPrivate::SeamlessVideoDecoderPrivate(SeamlessVideoDecoder *parent):
	QObject(parent),
	q_ptr(parent),
	frameNumber(0),
    decoderFrameOffset(0),
	metadataCache(kMetadataMaxSize)
{
	
}

void SeamlessVideoDecoderPrivate::addMetadata(const QnConstCompressedVideoDataPtr& frame)
{
    auto metadata = new FrameMetadata(frame);
    metadata->frameNum = frameNumber++;
    metadataCache.insert(metadata->frameNum, metadata);
}

const FrameMetadata* SeamlessVideoDecoderPrivate::findMetadata(int frameNum) const
{
	return metadataCache.object(frameNum);
}

void SeamlessVideoDecoderPrivate::clearMetadata()
{
	metadataCache.clear();
}

int SeamlessVideoDecoderPrivate::decoderFrameNumToLocalNum(int value) const
{
    return value + decoderFrameOffset;
}

// ----------------------------------- SeamlessVideoDecoder -----------------------------------

SeamlessVideoDecoder::SeamlessVideoDecoder():
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

bool SeamlessVideoDecoder::decode(const QnConstCompressedVideoDataPtr& frame, QnVideoFramePtr* result)
{
	Q_D(SeamlessVideoDecoder);
	if (result)
		result->clear();

	FrameBasicInfo frameInfo(frame);
    bool isSimilarParams = frameInfo.codec == d->prevFrameInfo.codec;
    if (!frameInfo.size.isEmpty())
        isSimilarParams &= frameInfo.size == d->prevFrameInfo.size;
    if (!isSimilarParams)
	{
        QnVideoFramePtr decodedFrame;
		if (d->videoDecoder)
		{
			while (1) 
			{
				int decodedFrameNum = d->videoDecoder->decode(QnConstCompressedVideoDataPtr(), &decodedFrame);
                if (!decodedFrame)
					break; //< decoder's buffer is flushed
                const FrameMetadata* metadata = d->findMetadata(d->decoderFrameNumToLocalNum(decodedFrameNum));
                if (metadata)
                    metadata->serialize(decodedFrame);
				d->queue.push_back(std::move(decodedFrame));
			}
		}
	
		d->videoDecoder = VideoDecoderRegistry::instance()->createCompatibleDecoder(frame);
        d->decoderFrameOffset = d->frameNumber;
		d->prevFrameInfo = frameInfo;
		d->clearMetadata();
	}
	
	d->addMetadata(frame);
	int decodedFrameNum = 0;
	if (d->videoDecoder) 
	{
        QnVideoFramePtr decodedFrame;
		decodedFrameNum = d->videoDecoder->decode(frame, &decodedFrame);
		if (decodedFrame) 
		{
            const FrameMetadata* metadata = d->findMetadata(d->decoderFrameNumToLocalNum(decodedFrameNum));
            if (metadata)
                metadata->serialize(decodedFrame);
			d->queue.push_back(std::move(decodedFrame));
		}
	}

	if (d->queue.empty())
		return decodedFrameNum >= 0; //< Return false if decoding fails and no queued frames are left.

	*result = d->queue.front();
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
