#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <stdint.h>
#include <QCoreApplication>

#include "transcoder.h"
#include "ffmpeg_audio_resampler.h"
#include <utils/common/byte_array.h>
#include <utils/media/ffmpeg_helper.h>

/**
 * Transcodes audio packets from one format to another.
 * Can be used to change codec and/or sample rate.
 */
class QnFfmpegAudioTranscoder: public QnAudioTranscoder
{
    Q_DECLARE_TR_FUNCTIONS(QnFfmpegAudioTranscoder)

public:
    /**
     * \param codecId Id of destination codec.
     */
    QnFfmpegAudioTranscoder(AVCodecID codecId);
    ~QnFfmpegAudioTranscoder();

    /**
     * \brief Transcodes packet from one codec/sample rate to another ones.
     * \param[in] media Encoded audio data that should be transcoded.
     * \param[out] result Pointer to media packet transcoded data will be written in.
     * Can be equal to nullptr after function returns if there is not enough data to create output
     * packet.
     * \return 0 on success, error code otherwise.
     */
    virtual int transcodePacket(
        const QnConstAbstractMediaDataPtr& media,
        QnAbstractMediaDataPtr* const result) override;

    /**
     * \brief Sets up decoder context and opens it.
     * \param[in] audio Encoded audio packet. It's required that proper decoding context was set for
     * that packet.
     * \return true on success, false in case of error
     */
    virtual bool open(const QnConstCompressedAudioDataPtr& audio) override;

    /**
     * \brief Sets up decoder context and opens it.
     * \param context Decoder context.
     * \return true on success, false in case of error
     */
    bool open(const QnConstMediaContextPtr& context);

    /**
     * \brief Tells if some amount of data can be encoded without providing additional packets.
     * \returns true if some encoded data can be pulled out of encoder, false otherwise
     */
    virtual bool existMoreData() const override;

    /**
     * \return true if last call for open returns true, false otherwise.
     */
    bool isOpened() const;

    /**
     * \return encoder context.
     */
    AVCodecContext* getCodecContext();

    /**
     * \brief Sets destination sample rate.
     * \param value Desired destination sample rate.
     */
    void setSampleRate(int value);

private:
    void tuneContextsWithMedia(
        AVCodecContext* inCtx,
        AVCodecContext* outCtx,
        const QnConstAbstractMediaDataPtr& media);

    std::size_t getSampleMultiplyCoefficient(const AVCodecContext* ctx);
    std::size_t getPlanesCount(const AVCodecContext* ctx);
    bool initResampler();

    QnAbstractMediaDataPtr createMediaDataFromAVPacket(const AVPacket& packet);

private:
    AVCodecContext* m_encoderCtx;
    AVCodecContext* m_decoderCtx;
    FfmpegAudioResampler m_resampler;
    bool m_resamplerInitialized = false;

    /** 
     * Wrapper for m_encoderContext.
     * Used to create our own media packets from AVPacket ffmpeg structure.
     */
    QnConstMediaContextPtr m_context;

    qint64 m_firstEncodedPts;
    qint64 m_lastTimestamp;
    int m_encodedDuration;
    int m_dstSampleRate;

    AVFrame* m_frameDecodeTo;
    bool m_isOpened;
};

typedef QSharedPointer<QnFfmpegAudioTranscoder> QnFfmpegAudioTranscoderPtr;

#endif // ENABLE_DATA_PROVIDERS

