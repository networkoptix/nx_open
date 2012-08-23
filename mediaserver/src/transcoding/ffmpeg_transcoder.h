#ifndef __FFMPEG_TRANSCODER_H
#define __FFMPEG_TRANSCODER_H

#include "libavcodec/avcodec.h"
#include "transcoder.h"
#include "utils/media/frame_info.h"
#include "decoders/video/ffmpeg.h"

class QnFfmpegTranscoder: public QnTranscoder
{
public:
    typedef QMap<QString, QVariant> Params;

    QnFfmpegTranscoder();
    ~QnFfmpegTranscoder();

    int setContainer(const QString& value);
protected:
    virtual int open(QnCompressedVideoDataPtr video, QnCompressedAudioDataPtr audio) override;
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
