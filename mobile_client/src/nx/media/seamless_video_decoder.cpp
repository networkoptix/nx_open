#include <deque>

#include "seamless_video_decoder.h"
#include "abstract_video_decoder.h"
#include "video_decoder_registry.h"

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

namespace nx
{
namespace media
{


class SeamlessVideoDecoderPrivate : public QObject
{
	Q_DECLARE_PUBLIC(SeamlessVideoDecoder)
	SeamlessVideoDecoder *q_ptr;
public:
	SeamlessVideoDecoderPrivate(SeamlessVideoDecoder *parent);
public:
	std::deque<QSharedPointer<QVideoFrame>> queue; //< temporary  buffer for decoded data
	VideoDecoderPtr videoDecoder;
	FrameBasicInfo prevFrameInfo;
};

SeamlessVideoDecoderPrivate::SeamlessVideoDecoderPrivate(SeamlessVideoDecoder *parent)
	: QObject(parent)
	, q_ptr(parent)
{

}

// ----------------------------------- SeamlessVideoDecoder -----------------------------------

SeamlessVideoDecoder::SeamlessVideoDecoder()
	: QObject()
	, d_ptr(new SeamlessVideoDecoderPrivate(this))
{
	Q_D(SeamlessVideoDecoder);
}

SeamlessVideoDecoder::~SeamlessVideoDecoder()
{
	Q_D(SeamlessVideoDecoder);
}

bool SeamlessVideoDecoder::decode(const QnConstCompressedVideoDataPtr& frame, QSharedPointer<QVideoFrame>* result)
{
	Q_D(SeamlessVideoDecoder);
	if (result)
		result->clear();

	FrameBasicInfo frameInfo(frame);
	if (!(frameInfo == d->prevFrameInfo))
	{
		QSharedPointer<QVideoFrame> decodedFrame;
		while (d->videoDecoder && d->videoDecoder->decode(QnConstCompressedVideoDataPtr(), &decodedFrame))
			d->queue.push_back(std::move(decodedFrame));
	
		d->videoDecoder = VideoDecoderRegistry::instance()->createCompatibleDecoder(frame);
		d->prevFrameInfo = frameInfo;
	}
	
	bool success = false;
	if (d->videoDecoder) 
	{
		QSharedPointer<QVideoFrame> decodedFrame;
		success = d->videoDecoder->decode(frame, &decodedFrame);
		if (success && decodedFrame)
			d->queue.push_back(std::move(decodedFrame));
	}

	if (d->queue.empty())
		return success; // return false if decode failure and no queued frame left

	*result = d->queue.front();
	d->queue.pop_front();
	return true;
}


}
}
