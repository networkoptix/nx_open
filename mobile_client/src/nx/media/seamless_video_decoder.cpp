#include <deque>

#include <QCache>

#include "seamless_video_decoder.h"
#include "abstract_video_decoder.h"
#include "video_decoder_registry.h"
#include "frame_metadata.h"

namespace
{
	static const int kMetadataMaxSize = 16;
}

namespace
{
	/** This data is used to compare current and previous frame so as to reset video decoder if need */
	struct FrameBasicInfo
	{
		bool operator== (const FrameBasicInfo& other) const {
			return other.size == size && other.codec == codec;
		}

		FrameBasicInfo(): 
			codec(CODEC_ID_NONE)
		{

		}
		
		FrameBasicInfo(const QnConstCompressedVideoDataPtr& frame):
			codec(CODEC_ID_NONE)
		{
			// todo: implement me
			codec = frame->compressionType;
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
public:
    std::deque<QnVideoFramePtr> queue; //< temporary  buffer for decoded data
	VideoDecoderPtr videoDecoder;
	FrameBasicInfo prevFrameInfo;
	int frameNumber;
	QCache<int, FrameMetadata> metadataCache;
};

SeamlessVideoDecoderPrivate::SeamlessVideoDecoderPrivate(SeamlessVideoDecoder *parent):
	QObject(parent),
	q_ptr(parent),
	frameNumber(0),
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

// ----------------------------------- SeamlessVideoDecoder -----------------------------------

SeamlessVideoDecoder::SeamlessVideoDecoder():
	QObject(),
	d_ptr(new SeamlessVideoDecoderPrivate(this))
{
}

SeamlessVideoDecoder::~SeamlessVideoDecoder()
{
}

bool SeamlessVideoDecoder::decode(const QnConstCompressedVideoDataPtr& frame, QnVideoFramePtr* result)
{
	Q_D(SeamlessVideoDecoder);
	if (result)
		result->clear();

	FrameBasicInfo frameInfo(frame);
	if (!(frameInfo == d->prevFrameInfo))
	{
        QnVideoFramePtr decodedFrame;
		if (d->videoDecoder)
		{
			while (1) 
			{
				int decodedFrameNum = d->videoDecoder->decode(QnConstCompressedVideoDataPtr(), &decodedFrame);
                if (!decodedFrame)
					break; //< decoder's buffer is flushed
				const FrameMetadata* metadata = d->findMetadata(decodedFrameNum);
                if (metadata)
                    metadata->serialize(decodedFrame);
				d->queue.push_back(std::move(decodedFrame));
			}
		}
	
		d->videoDecoder = VideoDecoderRegistry::instance()->createCompatibleDecoder(frame);
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
			const FrameMetadata* metadata = d->findMetadata(decodedFrameNum);
            if (metadata)
                metadata->serialize(decodedFrame);
			d->queue.push_back(std::move(decodedFrame));
		}
	}

	if (d->queue.empty())
		return decodedFrameNum >= 0; //< return false if decode failure and no queued frame left

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
