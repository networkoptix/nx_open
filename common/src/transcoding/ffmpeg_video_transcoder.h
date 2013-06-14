#ifndef __FFMPEG_VIDEO_TRANSCODER_H__
#define __FFMPEG_VIDEO_TRANSCODER_H__

#include "transcoder.h"

extern "C"
{
    #include <libswscale/swscale.h>
}

#include "utils/media/frame_info.h"
#include "decoders/video/ffmpeg.h"

class QnFfmpegVideoTranscoder: public QnVideoTranscoder
{
public:
    QnFfmpegVideoTranscoder(CodecID codecId);
    ~QnFfmpegVideoTranscoder();

    virtual int transcodePacket(QnAbstractMediaDataPtr media, QnAbstractMediaDataPtr* const result) override;
    virtual bool open(QnCompressedVideoDataPtr video) override;
    AVCodecContext* getCodecContext();

    /* Allow multithread transcoding */
    void setMTMode(bool value);
private:
    int rescaleFrame(CLVideoDecoderOutput* decodedFrame, const QRectF& dstRectF, int ch);
private:
    QVector<CLFFmpegVideoDecoder*> m_videoDecoders;
    QSharedPointer<CLVideoDecoderOutput> m_decodedVideoFrame;
    CLVideoDecoderOutput m_scaledVideoFrame;
    CLVideoDecoderOutput m_decodedFrameRect;

    

    quint8* m_videoEncodingBuffer;
    AVCodecContext* m_encoderCtx;

    SwsContext* scaleContext[CL_MAX_CHANNELS];
    int m_lastSrcWidth[CL_MAX_CHANNELS];
    int m_lastSrcHeight[CL_MAX_CHANNELS];

    qint64 m_firstEncodedPts;
    bool m_mtMode;
};

typedef QSharedPointer<QnFfmpegVideoTranscoder> QnFfmpegVideoTranscoderPtr;

#endif // __FFMPEG_VIDEO_TRANSCODER_H__
