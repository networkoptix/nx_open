// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <stdint.h>

#include <nx/media/audio_data_packet.h>
#include <nx/media/ffmpeg_helper.h>
#include <nx/utils/byte_array.h>
#include <transcoding/abstract_codec_transcoder.h>
#include <transcoding/ffmpeg_audio_resampler.h>
#include <transcoding/audio_encoder.h>

//< Minimal supported sample rate in mp4(with mp3), see ffmpeg(7.0.1) movenc.c:7579
constexpr int kMinMp4Mp3SampleRate = 16000;

/**
 * Transcodes audio packets from one format to another.
 * Can be used to change codec and/or sample rate.
 */
class NX_VMS_COMMON_API QnFfmpegAudioTranscoder: public AbstractCodecTranscoder
{
public:
    struct Config
    {
        AVCodecID targetCodecId = AV_CODEC_ID_NONE;
        int bitrate = -1;
        // Sets destination frame size.
        int dstFrameSize = 0;
    };

public:
    /**
     * \param codecId Id of destination codec.
     */
    QnFfmpegAudioTranscoder(const Config& config);
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
    bool open(const QnConstCompressedAudioDataPtr& audio);

    /**
     * \brief Sets up decoder context and opens it.
     * \param context Decoder context.
     * \return true on success, false in case of error
     */
    bool open(const CodecParametersConstPtr& context);

    void close();

    /**
     * \return true if last call for open returns true, false otherwise.
     */
    bool isOpened() const;

    AVCodecParameters* getCodecParameters();

    /**
     * \brief Sets destination sample rate.
     * \param value Desired destination sample rate.
     */
    void setSampleRate(int value);
    bool sendPacket(const QnConstAbstractMediaDataPtr& media);
    bool receivePacket(QnAbstractMediaDataPtr* const result);

private:
    bool openDecoder(const CodecParametersConstPtr& context);

private:
    const Config m_config;
    AVCodecContext* m_decoderCtx = nullptr;
    nx::media::ffmpeg::AudioEncoder m_encoder;

    bool m_isOpened = false;
    int m_channelNumber = 0;
};

using QnFfmpegAudioTranscoderPtr = std::unique_ptr<QnFfmpegAudioTranscoder>;
