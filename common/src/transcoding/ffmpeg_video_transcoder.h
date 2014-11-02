#ifndef __FFMPEG_VIDEO_TRANSCODER_H__
#define __FFMPEG_VIDEO_TRANSCODER_H__

#ifdef ENABLE_DATA_PROVIDERS

#include <QCoreApplication>

#include "transcoder.h"

extern "C"
{
    #include <libswscale/swscale.h>
}

#include "utils/media/frame_info.h"
#include "decoders/video/ffmpeg.h"

class QnFfmpegVideoTranscoder: public QnVideoTranscoder
{
    Q_DECLARE_TR_FUNCTIONS(QnFfmpegVideoTranscoder)
public:
    QnFfmpegVideoTranscoder(CodecID codecId);
    ~QnFfmpegVideoTranscoder();

    virtual int transcodePacket(const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result) override;
    virtual bool open(const QnConstCompressedVideoDataPtr& video) override;
    void close();
    AVCodecContext* getCodecContext();

    /* Allow multithread transcoding */
    void setMTMode(bool value);
    virtual void addFilter(QnAbstractImageFilter* filter) override;
private:
    QVector<CLFFmpegVideoDecoder*> m_videoDecoders;
    CLVideoDecoderOutputPtr m_decodedVideoFrame;

    quint8* m_videoEncodingBuffer;
    AVCodecContext* m_encoderCtx;

    int m_lastSrcWidth[CL_MAX_CHANNELS];
    int m_lastSrcHeight[CL_MAX_CHANNELS];

    qint64 m_firstEncodedPts;
    bool m_mtMode;
};

typedef QSharedPointer<QnFfmpegVideoTranscoder> QnFfmpegVideoTranscoderPtr;

#endif // ENABLE_DATA_PROVIDERS

#endif // __FFMPEG_VIDEO_TRANSCODER_H__
