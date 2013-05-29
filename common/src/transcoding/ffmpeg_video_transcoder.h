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
    enum OnScreenDatePos {Date_None, Date_LeftTop, Date_RightTop, Date_RightBottom, Date_LeftBottom};

    QnFfmpegVideoTranscoder(CodecID codecId);
    ~QnFfmpegVideoTranscoder();

    virtual int transcodePacket(QnAbstractMediaDataPtr media, QnAbstractMediaDataPtr* const result) override;
    virtual bool open(QnCompressedVideoDataPtr video) override;
    AVCodecContext* getCodecContext();

    /* Allow multithread transcoding */
    void setMTMode(bool value);

    /* Draw video frames time on the screen */
    void setDrawDateTime(OnScreenDatePos value);

    void setOnScreenDateOffset(int timeOffsetMs);

private:
    int rescaleFrame(CLVideoDecoderOutput* decodedFrame);
    void doDrawOnScreenTime(CLVideoDecoderOutput* frame);
    void initTimeDrawing(CLVideoDecoderOutput* frame, const QString& timeStr);
private:
    CLFFmpegVideoDecoder* m_videoDecoder;
    QSharedPointer<CLVideoDecoderOutput> m_decodedVideoFrame;
    CLVideoDecoderOutput m_scaledVideoFrame;
    CLVideoDecoderOutput m_decodedFrameRect;
    quint8* m_videoEncodingBuffer;
    AVCodecContext* m_encoderCtx;
    SwsContext* scaleContext;
    qint64 m_firstEncodedPts;
    int m_lastSrcWidth;
    int m_lastSrcHeight;
    bool m_mtMode;
    
    OnScreenDatePos m_dateTextPos;
    uchar* m_imageBuffer;
    QImage* m_timeImg;
    QFont m_timeFont;
    int m_dateTimeXOffs;
    int m_dateTimeYOffs;
    int m_onscreenDateOffset;

    int m_bufXOffs;
    int m_bufYOffs;
};

typedef QSharedPointer<QnFfmpegVideoTranscoder> QnFfmpegVideoTranscoderPtr;

#endif // __FFMPEG_VIDEO_TRANSCODER_H__
