#ifndef __FFMPEG_AUDIO_TRANSCODER_H__
#define __FFMPEG_AUDIO_TRANSCODER_H__

#ifdef ENABLE_DATA_PROVIDERS

extern "C" {
#include <libswresample/swresample.h>
}

#include <QCoreApplication>
#include <stdint.h>

#include "transcoder.h"
#include "utils/common/byte_array.h"
#include <utils/media/ffmpeg_helper.h>



class QnFfmpegAudioTranscoder: public QnAudioTranscoder
{
    Q_DECLARE_TR_FUNCTIONS(QnFfmpegAudioTranscoder)
public:
    QnFfmpegAudioTranscoder(AVCodecID codecId);
    ~QnFfmpegAudioTranscoder();

    //virtual int transcodePacket(const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result) override;
    virtual int transcodePacket(const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result);
    virtual bool open(const QnConstCompressedAudioDataPtr& audio) override;
    bool open(const QnConstMediaContextPtr& context);
    bool isOpened() const;
    AVCodecContext* getCodecContext();
    virtual bool existMoreData() const override;
    void setSampleRate(int value);

private:
    void initResampleCtx(const AVCodecContext* inCtx, const AVCodecContext* outCtx);
    void allocSampleBuffers(
        const AVCodecContext* inCtx,
        const AVCodecContext* outCtx,
        const SwrContext* resampleCtx);

    void shiftSamplesResidue(
        const AVCodecContext* ctx,
        uint8_t* const* const  buffersBase,
        const std::size_t samplesOffset,
        const std::size_t samplesPerChannelCount);

    void fillFrameWithSamples(
        AVFrame* frame,
        uint8_t** sampleBuffers,
        const std::size_t offset,
        const AVCodecContext* encoderCtx);

    QnAbstractMediaDataPtr createMediaDataFromAVPacket(const AVPacket& packet);

private:
    quint8* m_audioEncodingBuffer;
    AVCodecContext* m_encoderCtx;
    AVCodecContext* m_decoderCtx;
    qint64 m_firstEncodedPts;

    QnByteArray m_unresampledData;
    QnByteArray m_resampledData;

    qint64 m_lastTimestamp;
    QnConstMediaContextPtr m_context;

    bool m_downmixAudio;
    int m_frameNum;
    //ReSampleContext* m_resampleCtx;
    SwrContext* m_resampleCtx;
    uint8_t** m_sampleBuffers;
    std::size_t m_currentSampleCount;
    std::size_t m_currentSampleBufferOffset;

    int m_dstSampleRate;
    
    std::unique_ptr<QnFfmpegAudioHelper> m_audioHelper;

    AVFrame* m_frameDecodeTo;
    AVFrame* m_frameToEncode;
};

typedef QSharedPointer<QnFfmpegAudioTranscoder> QnFfmpegAudioTranscoderPtr;

#endif // ENABLE_DATA_PROVIDERS

#endif // __FFMPEG_AUDIO_TRANSCODER_H__

