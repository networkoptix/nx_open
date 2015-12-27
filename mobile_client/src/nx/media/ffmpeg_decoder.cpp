#include "ffmpeg_decoder.h"

extern "C"
{
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
}

#include <utils/media/ffmpeg_helper.h>
#include <utils/thread/mutex.h>

#include "aligned_mem_video_buffer.h"

namespace
{
	int kMediaAlignment = 32;
}

namespace nx
{
namespace media
{

class InitFfmpegLib
{
public:

	InitFfmpegLib()
	{
		av_register_all();
		if (av_lockmgr_register(&InitFfmpegLib::lockmgr) != 0)
			qCritical() << "Failed to register ffmpeg lock manager";
	}
	
	static int lockmgr(void **mtx, enum AVLockOp op)
	{
		QnMutex** qMutex = (QnMutex**)mtx;
		switch (op) {
		case AV_LOCK_CREATE:
			*qMutex = new QnMutex();
			return 0;
		case AV_LOCK_OBTAIN:
			(*qMutex)->lock();
			return 0;
		case AV_LOCK_RELEASE:
			(*qMutex)->unlock();
			return 0;
		case AV_LOCK_DESTROY:
			delete *qMutex;
			return 0;
		}
		return 1;
	}

};

class FfmpegDecoderPrivate : public QObject
{
	Q_DECLARE_PUBLIC(FfmpegDecoder)
	FfmpegDecoder *q_ptr;
public:
	FfmpegDecoderPrivate() 
		: codecContext(nullptr)
		, frame(avcodec_alloc_frame())
		, scaleContext(nullptr)
		, lastPTS(AV_NOPTS_VALUE)
	{
	}
	~FfmpegDecoderPrivate() { 
		closeCodecContext();
		av_free(frame);
		sws_freeContext(scaleContext);
	}

	void initContext(const QnConstCompressedVideoDataPtr& frame);
	void closeCodecContext();

	AVCodecContext* codecContext;
	AVFrame* frame;
	SwsContext* scaleContext;
	qint64 lastPTS;
};

void FfmpegDecoderPrivate::initContext(const QnConstCompressedVideoDataPtr& frame)
{
	if (!frame)
		return;

	auto codec = avcodec_find_decoder(frame->compressionType);
	codecContext = avcodec_alloc_context3(codec);
	if (frame->context)
		QnFfmpegHelper::mediaContextToAvCodecContext(codecContext, frame->context);
	//codecContext->thread_count = 4;
	if (avcodec_open2(codecContext, codec, nullptr) < 0)
	{
		qWarning() << "Can't open decoder for codec" << frame->compressionType;
		closeCodecContext();
		return;
	}
}

void FfmpegDecoderPrivate::closeCodecContext()
{
	QnFfmpegHelper::deleteAvCodecContext(codecContext);
	codecContext = 0;
}

// ---------------------- FfmpegDecoder ----------------------

FfmpegDecoder::FfmpegDecoder()
	: AbstractVideoDecoder()
	, d_ptr(new FfmpegDecoderPrivate())

{
	static InitFfmpegLib init;
}

FfmpegDecoder::~FfmpegDecoder()
{
}

bool FfmpegDecoder::isCompatible(const QnConstCompressedVideoDataPtr& frame)
{
	// todo: implement me
    Q_UNUSED(frame);
	return true;
}

QVideoFrame::PixelFormat toQtPixelFormat(int ffmpegFormat)
{
	switch (ffmpegFormat)
	{
		case PIX_FMT_YUV420P:
			return QVideoFrame::Format_YUV420P;
		case PIX_FMT_YUYV422:
			return QVideoFrame::Format_UYVY; //< todo: probable pixel shuffle is required here
		case PIX_FMT_YUV444P:
			return QVideoFrame::Format_YUV444;
		default:
			return QVideoFrame::Format_Invalid;
	}
}

int FfmpegDecoder::decode(const QnConstCompressedVideoDataPtr& frame, QnVideoFramePtr* result)
{
	Q_D(FfmpegDecoder);

	if (!d->codecContext) 
	{
		d->initContext(frame);
		if (!d->codecContext)
			return -1;
	}

	AVPacket avpkt;
	av_init_packet(&avpkt);
	if (frame)
	{
		avpkt.data = (unsigned char*)frame->data();
		avpkt.size = static_cast<int>(frame->dataSize());
		avpkt.dts = avpkt.pts = frame->timestamp;
		if (frame->flags & QnAbstractMediaData::MediaFlags_AVKey)
			avpkt.flags = AV_PKT_FLAG_KEY;

		//  It's already guaranteed by QnByteArray there is an extra space reserved. We must fill padding bytes according to ffmpeg documentation
		if (avpkt.data)
			memset(avpkt.data + avpkt.size, 0, FF_INPUT_BUFFER_PADDING_SIZE);
	}
	else 
	{
		// there is known a ffmpeg bug. It returns below time for the very last packet while flusing internal buffer.
		// So, repeat this time for the empty packet in order to aviod the bug
		avpkt.pts = avpkt.dts = d->lastPTS;
	}


	int gotData = 0;
	avcodec_decode_video2(d->codecContext, d->frame, &gotData, &avpkt);
	if (gotData <= 0)
		return gotData; //< negative value means error. zerro value is buffering
	
	/*
	QMemoryVideoBuffer buffer(QByteArray((const char*) d->frame->data, d->frame->width * d->frame->height * 1.5), d->frame->linesize[0]);
	result->reset(new QVideoFrame(
		&buffer,
		QSize(d->frame->width, d->frame->height), 
		toQtPixelFormat(d->frame->format))
	);
	*/

	if (!d->scaleContext)
	{
		d->scaleContext = sws_getContext(d->frame->width, d->frame->height, (PixelFormat)d->frame->format,
										 d->frame->width, d->frame->height, PIX_FMT_BGRA,
			                             SWS_BICUBIC, NULL, NULL, NULL);
	}
	const int alignedWidth = qPower2Ceil((unsigned)d->frame->width, (unsigned)kMediaAlignment);
	const int numBytes = avpicture_get_size(PIX_FMT_BGRA, alignedWidth, d->frame->height);
	const int argbLineSize = alignedWidth * 4;
	
	auto alignedBuffer = new AlignedMemVideoBuffer(numBytes, kMediaAlignment, argbLineSize);
	auto videoFrame = new QVideoFrame(alignedBuffer, QSize(d->frame->width, d->frame->height), QVideoFrame::Format_RGB32);
	videoFrame->setStartTime(d->frame->pkt_dts / 1000); //< ffmpeg pts/dst are mixed up here, so it's pkt_dts. Also Convert usec to msec.
		
	videoFrame->map(QAbstractVideoBuffer::WriteOnly);
	uchar* buffer = videoFrame->bits();
	
	quint8* dstData[4];
	memset(dstData, 0, sizeof(dstData));
	dstData[0] = (quint8*) buffer;

	int dstLinesize[4];
	memset(dstLinesize, 0, sizeof(dstLinesize));
	dstLinesize[0] = argbLineSize;
	
	sws_scale(d->scaleContext, d->frame->data, d->frame->linesize, 0, d->frame->height, dstData, dstLinesize);
	videoFrame->unmap();

	result->reset(videoFrame);
	
	return d->frame->coded_picture_number;
}

}
}
