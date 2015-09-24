#ifndef __FFMPEG_AUDIO_TRANSCODER_H__
#define __FFMPEG_AUDIO_TRANSCODER_H__

#ifdef ENABLE_DATA_PROVIDERS

#include <QCoreApplication>

#include "transcoder.h"
#include "utils/common/byte_array.h"

class QnFfmpegAudioTranscoder: public QnAudioTranscoder
{
    Q_DECLARE_TR_FUNCTIONS(QnFfmpegAudioTranscoder)
public:
    QnFfmpegAudioTranscoder(CodecID codecId);
    ~QnFfmpegAudioTranscoder();

    virtual int transcodePacket(const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result) override;
    virtual bool open(const QnConstCompressedAudioDataPtr& audio) override;
    bool open(const QnMediaContextPtr& codecCtx);
    AVCodecContext* getCodecContext();
    virtual bool existMoreData() const override;
private:
    quint8* m_audioEncodingBuffer;
    quint8* m_resampleBuffer;
    AVCodecContext* m_encoderCtx;
    AVCodecContext* m_decoderContext;
    qint64 m_firstEncodedPts;

    quint8* m_decodedBuffer;
    int m_decodedBufferSize;
    qint64 m_lastTimestamp;
    QnMediaContextPtr m_context;
    
    bool m_downmixAudio;
    int m_frameNum;
    ReSampleContext* m_resampleCtx;
};

typedef QSharedPointer<QnFfmpegAudioTranscoder> QnFfmpegAudioTranscoderPtr;

#endif // ENABLE_DATA_PROVIDERS

#endif // __FFMPEG_AUDIO_TRANSCODER_H__

