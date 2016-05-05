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
    bool open(const QnConstMediaContextPtr& context);
    bool isOpened() const;
    AVCodecContext* getCodecContext();
    virtual bool existMoreData() const override;
    void setSampleRate(int value);
private:
    quint8* m_audioEncodingBuffer;
    AVCodecContext* m_encoderCtx;
    AVCodecContext* m_decoderContext;
    qint64 m_firstEncodedPts;

    QnByteArray m_unresampledData;
    QnByteArray m_resampledData;

    qint64 m_lastTimestamp;
    QnConstMediaContextPtr m_context;
    
    bool m_downmixAudio;
    int m_frameNum;
    ReSampleContext* m_resampleCtx;
    int m_dstSampleRate;
};

typedef QSharedPointer<QnFfmpegAudioTranscoder> QnFfmpegAudioTranscoderPtr;

#endif // ENABLE_DATA_PROVIDERS

#endif // __FFMPEG_AUDIO_TRANSCODER_H__

