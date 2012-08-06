#ifndef __FFMPEG_TRANSCODER_H
#define __FFMPEG_TRANSCODER_H

#include "libavcodec/avcodec.h"
#include "transcoder.h"
#include "utils/media/frame_info.h"
#include "decoders/video/ffmpeg.h"

const static int MAX_VIDEO_FRAME = 1024 * 3;

class QnFfmpegVideoTranscoder: public QnVideoTranscoder
{
public:
    QnFfmpegVideoTranscoder();
    ~QnFfmpegVideoTranscoder();

    virtual int transcodePacket(QnAbstractMediaDataPtr media, QnAbstractMediaDataPtr& result) override;
private:
    CLFFmpegVideoDecoder* m_videoDecoder;
    CLVideoDecoderOutput m_decodedVideoFrame;
    quint8* m_videoEncodingBuffer;
    QnMediaContextPtr m_encoderCtx;
};

class QnFfmpegTranscoder: public QnTranscoder
{
public:
    typedef QMap<QString, QVariant> Params;

    QnFfmpegTranscoder();
    ~QnFfmpegTranscoder();

    int setContainer(const QString& value);

    virtual bool setVideoCodec(CodecID codec, QnVideoTranscoderPtr vTranscoder = QnVideoTranscoderPtr()) override;


    virtual bool setAudioCodec(CodecID codec, QnAudioTranscoderPtr aTranscoder = QnAudioTranscoderPtr()) override;

    int transcodePacket(QnAbstractMediaDataPtr media, QnByteArray& result);

    QString getLastErrorMessage() const;

    qint32 write(quint8* buf, int size);
protected:
    virtual int open(QnCompressedVideoDataPtr video, QnCompressedAudioDataPtr audio) override;
    virtual int transcodePacketInternal(QnAbstractMediaDataPtr media, QnByteArray& result) override;
private:
    friend qint32 ffmpegWritePacket(void *opaque, quint8* buf, int size);
    AVIOContext* createFfmpegIOContext();
    void closeFfmpegContext();
private:
    AVCodecContext* m_videoEncoderCodecCtx;
    AVCodecContext* m_audioEncoderCodecCtx;
    int m_videoBitrate;
    qint64 m_startDateTime;
    AVFormatContext* m_formatCtx;
    QString m_lastErrMessage;
   
    QnByteArray* m_dstByteArray;
    AVIOContext* m_ioContext;
    QString m_container;
};

#endif  // __FFMPEG_TRANSCODER_H
