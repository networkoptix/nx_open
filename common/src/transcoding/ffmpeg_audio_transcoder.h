#ifndef __FFMPEG_AUDIO_TRANSCODER_H__
#define __FFMPEG_AUDIO_TRANSCODER_H__

#include <QCoreApplication>

#include "transcoder.h"
#include "utils/common/byte_array.h"

class QnFfmpegAudioTranscoder: public QnAudioTranscoder
{
    Q_DECLARE_TR_FUNCTIONS(QnFfmpegAudioTranscoder)
public:
    QnFfmpegAudioTranscoder(CodecID codecId);
    ~QnFfmpegAudioTranscoder();

    virtual int transcodePacket(QnConstAbstractMediaDataPtr media, QnAbstractMediaDataPtr* const result) override;
    virtual bool open(QnConstCompressedAudioDataPtr audio) override;
    bool open(QnMediaContextPtr codecCtx);
    AVCodecContext* getCodecContext();
private:
    quint8* m_audioEncodingBuffer;
    AVCodecContext* m_encoderCtx;
    AVCodecContext* m_decoderContext;
    qint64 m_firstEncodedPts;

    quint8* m_decodedBuffer;
    int m_decodedBufferSize;
    qint64 m_lastTimestamp;
    QnMediaContextPtr m_context;
    
    bool m_downmixAudio;
    int m_frameNum;
};

typedef QSharedPointer<QnFfmpegAudioTranscoder> QnFfmpegAudioTranscoderPtr;

#endif // __FFMPEG_AUDIO_TRANSCODER_H__
