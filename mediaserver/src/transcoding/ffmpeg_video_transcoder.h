#ifndef __FFMPEG_VIDEO_TRANSCODER_H__
#define __FFMPEG_VIDEO_TRANSCODER_H__

#include "transcoder.h"
#include "utils/media/frame_info.h"
#include "decoders/video/ffmpeg.h"

class QnFfmpegVideoTranscoder: public QnVideoTranscoder
{
public:
    QnFfmpegVideoTranscoder(CodecID codecId);
    ~QnFfmpegVideoTranscoder();

    virtual int transcodePacket(QnAbstractMediaDataPtr media, QnAbstractMediaDataPtr& result) override;
private:
    int rescaleFrame();
private:
    CLFFmpegVideoDecoder* m_videoDecoder;
    CLVideoDecoderOutput m_decodedVideoFrame;
    CLVideoDecoderOutput m_scaledVideoFrame;
    quint8* m_videoEncodingBuffer;
    AVCodecContext* m_encoderCtx;
    SwsContext* scaleContext;
    qint64 m_firstEncodedPts;

};

#endif // __FFMPEG_VIDEO_TRANSCODER_H__
