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
    virtual bool open(QnCompressedVideoDataPtr video) override;
    AVCodecContext* getCodecContext();
private:
    int rescaleFrame();
private:
    CLFFmpegVideoDecoder* m_videoDecoder;
    QSharedPointer<CLVideoDecoderOutput> m_decodedVideoFrame;
    CLVideoDecoderOutput m_scaledVideoFrame;
    quint8* m_videoEncodingBuffer;
    AVCodecContext* m_encoderCtx;
    SwsContext* scaleContext;
    qint64 m_firstEncodedPts;
    int m_lastSrcWidth;
    int m_lastSrcHeight;
};

typedef QSharedPointer<QnFfmpegVideoTranscoder> QnFfmpegVideoTranscoderPtr;

#endif // __FFMPEG_VIDEO_TRANSCODER_H__
