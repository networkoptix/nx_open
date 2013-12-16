#ifndef __FFMPEG_TRANSCODER_H
#define __FFMPEG_TRANSCODER_H

extern "C"
{
    #include <libavcodec/avcodec.h>
}

#include "transcoder.h"
#include "utils/media/frame_info.h"
#include "decoders/video/ffmpeg.h"


class QnFfmpegTranscoder: public QnTranscoder
{
    Q_OBJECT;

public:
    static const int MTU_SIZE = 1412;

    QnFfmpegTranscoder();
    ~QnFfmpegTranscoder();

    int setContainer(const QString& value);

    AVCodecContext* getVideoCodecContext() const;
    AVCodecContext* getAudioCodecContext() const;
    AVFormatContext* getFormatContext() const { return m_formatCtx; }

    virtual int open(QnConstCompressedVideoDataPtr video, QnConstCompressedAudioDataPtr audio) override;

    //!Implementation of QnTranscoder::addTag
    virtual bool addTag( const QString& name, const QString& value ) override;

protected:
    virtual int transcodePacketInternal(QnConstAbstractMediaDataPtr media, QnByteArray* const result) override;

private:
    //friend qint32 ffmpegWritePacket(void *opaque, quint8* buf, int size);
    AVIOContext* createFfmpegIOContext();
    void closeFfmpegContext();

private:
    AVCodecContext* m_videoEncoderCodecCtx;
    AVCodecContext* m_audioEncoderCodecCtx;
    int m_videoBitrate;
    AVFormatContext* m_formatCtx;
    QString m_lastErrMessage;
   
    AVIOContext* m_ioContext;
    QString m_container;
    qint64 m_baseTime;
};

#endif  // __FFMPEG_TRANSCODER_H
