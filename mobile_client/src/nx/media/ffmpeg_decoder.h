#pragma once

#ifndef DISABLE_FFMPEG

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>

#include <nx/streaming/video_data_packet.h>

#include "abstract_video_decoder.h"

namespace nx {
namespace media {

/*
* This class implements ffmpeg video decoder
*/
class FfmpegDecoderPrivate;
class FfmpegDecoder : public AbstractVideoDecoder
{
public:
	FfmpegDecoder();
	virtual ~FfmpegDecoder();

    static bool isCompatible(const CodecID codec, const QSize& resolution);
    virtual int decode(const QnConstCompressedVideoDataPtr& frame, QnVideoFramePtr* result = nullptr) override;
private:
    void ffmpegToQtVideoFrame(QnVideoFramePtr* result);
private:
	QScopedPointer<FfmpegDecoderPrivate> d_ptr;
	Q_DECLARE_PRIVATE(FfmpegDecoder);
};

}
}

#endif // #DISABLE_FFMPEG
