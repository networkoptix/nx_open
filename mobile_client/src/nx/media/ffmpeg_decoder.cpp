#include "ffmpeg_decoder.h"

extern "C"
{
	#include <libavformat/avformat.h>
}

#include <utils/media/ffmpeg_helper.h>
#include <utils/thread/mutex.h>

#include "../qt5/src/qtmultimedia/src/multimedia/video/qmemoryvideobuffer_p.h"

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
		, lastPTS(AV_NOPTS_VALUE)
	{
	}
	~FfmpegDecoderPrivate() { 
		closeCodecContext();
		av_free(frame);
	}

	void initContext(const QnConstCompressedVideoDataPtr& frame);
	void closeCodecContext();

	AVCodecContext* codecContext;
	AVFrame* frame;
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
	if (avcodec_open2(codecContext, codec, nullptr) < 0)
	{
		qWarning() << "Can't open decoder for codec" << frame->compressionType;
		closeCodecContext();
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
	Q_D(FfmpegDecoder);
}

bool FfmpegDecoder::isCompatible(const QnConstCompressedVideoDataPtr& frame)
{
	// todo: implement me
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

bool FfmpegDecoder::decode(const QnConstCompressedVideoDataPtr& frame, QSharedPointer<QVideoFrame>* result)
{
	Q_D(FfmpegDecoder);

	if (!d->codecContext) 
	{
		d->initContext(frame);
		if (!d->codecContext)
			return false;
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
		return gotData == 0; //< negative value means error. zerro value is buffering
	
	QMemoryVideoBuffer buffer(QByteArray((const char*) d->frame->data, d->frame->width * d->frame->height * 1.5), d->frame->linesize[0]);
	result->reset(new QVideoFrame(
		&buffer,
		QSize(d->frame->width, d->frame->height), 
		toQtPixelFormat(d->frame->format))
	);

	return true;
}

}
}
