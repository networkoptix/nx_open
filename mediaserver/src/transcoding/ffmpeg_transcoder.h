#ifndef __FFMPEG_TRANSCODER_H
#define __FFMPEG_TRANSCODER_H

#include "libavcodec/avcodec.h"
#include "transcoder.h"
#include "utils/media/frame_info.h"
#include "decoders/video/ffmpeg.h"

class QnFfmpegTranscoder: public QnTranscoder
{
public:
    static const int MTU_SIZE = 1412;

    typedef QMap<QString, QVariant> Params;

    QnFfmpegTranscoder();
    ~QnFfmpegTranscoder();

    int setContainer(const QString& value);

    AVCodecContext* getVideoCodecContext() const { return m_videoEncoderCodecCtx; }
    AVCodecContext* getAudioCodecContext() const { return m_videoEncoderCodecCtx; }
    AVFormatContext* getFormatContext() const { return m_formatCtx; }

    virtual int open(QnCompressedVideoDataPtr video, QnCompressedAudioDataPtr audio) override;
protected:
    virtual int transcodePacketInternal(QnAbstractMediaDataPtr media, QnByteArray& result) override;
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
};

#endif  // __FFMPEG_TRANSCODER_H
