// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_audio_transcoder.h"

#include <nx/media/audio_data_packet.h>
#include <nx/media/codec_parameters.h>
#include <nx/media/config.h>
#include <nx/media/ffmpeg/ffmpeg_utils.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <transcoding/transcoding_utils.h>

QnFfmpegAudioTranscoder::QnFfmpegAudioTranscoder(const Config& config):
    m_config(config)
{
}

QnFfmpegAudioTranscoder::~QnFfmpegAudioTranscoder()
{
    close();
}

void QnFfmpegAudioTranscoder::close()
{
    if (m_decoderCtx)
        avcodec_free_context(&m_decoderCtx);

    m_isOpened = false;
}

bool QnFfmpegAudioTranscoder::open(const QnConstCompressedAudioDataPtr& audio)
{
    if (!audio || !audio->context)
    {
        NX_WARNING(this, "Audio context was not specified.");
        return false;
    }
    return open(audio->context);
}

bool QnFfmpegAudioTranscoder::open(const CodecParametersConstPtr& context)
{
    NX_ASSERT(context);
    auto codecpar = context->getAvCodecParameters();
    if (!m_encoder.open(
        m_config.targetCodecId,
        codecpar->sample_rate,
        (AVSampleFormat)codecpar->format,
        codecpar->ch_layout,
        m_config.bitrate,
        m_config.dstFrameSize))
    {
        return false;
    }
    return openDecoder(context);
}

bool QnFfmpegAudioTranscoder::openDecoder(const CodecParametersConstPtr& context)
{
    close();
    auto avCodec = avcodec_find_decoder(context->getCodecId());
    if (!avCodec)
    {
        NX_WARNING(this, "Could not find decoder for codec %1.", context->getCodecId());
        return false;
    }

    m_decoderCtx = avcodec_alloc_context3(0);
    context->toAvCodecContext(m_decoderCtx);
    if (avcodec_open2(m_decoderCtx, avCodec, 0) < 0)
    {
        NX_WARNING(this, "Could not initialize audio decoder.");
        return false;
    }

    m_isOpened = true;
    return true;
}

AVCodecParameters* QnFfmpegAudioTranscoder::getCodecParameters()
{
    return m_encoder.codecParameters() ? m_encoder.codecParameters()->getAvCodecParameters() : nullptr;
}

bool QnFfmpegAudioTranscoder::isOpened() const
{
    return m_isOpened;
}

int QnFfmpegAudioTranscoder::transcodePacket(
    const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result)
{
    if (m_decoderCtx && media && m_decoderCtx->codec_id != media->compressionType)
    {
        NX_DEBUG(this, "Input audio codec changed from: %1 to %2",
            m_decoderCtx->codec_id, media->compressionType);

        if (!openDecoder(media->context))
        {
            NX_WARNING(this,
                "Failed to reinitialize audio transcoder when codec changed from: %1 to %2",
                m_decoderCtx->codec_id,
                media->compressionType);
            return -1;
        }
    }
    if (!m_isOpened)
        return -1;

    if (result)
        result->reset();

    if (media && media->dataType != QnAbstractMediaData::DataType::AUDIO)
        return 0;

    if (media && !sendPacket(media))
        return -1;

    return receivePacket(result) ? 0 : -1;
}

bool QnFfmpegAudioTranscoder::sendPacket(const QnConstAbstractMediaDataPtr& media)
{
    m_channelNumber = media->channelNumber;

    // 1. push media to decoder
    auto avPacket = av_packet_alloc();
    avPacket->data = (uint8_t*)(media->data());
    avPacket->size = media->dataSize();
    avPacket->dts = media->timestamp;
    avPacket->pts = media->timestamp;

    int error = avcodec_send_packet(m_decoderCtx, avPacket);
    av_packet_free(&avPacket);

    if (error && error != AVERROR_EOF)
    {
        NX_WARNING(this, "ffmpeg audio decoder error: %1", nx::media::ffmpeg::avErrorToString(error));
        return false;
    }

    // 2. get media from decoder
    while (true)
    {
        AVFrame* decodedFrame = av_frame_alloc();
        if (!decodedFrame)
        {
            NX_ERROR(this, "Failed to transcode audio packet: out of memory");
            return false;
        }
        auto guard = nx::utils::makeScopeGuard([&decodedFrame]() {av_frame_free(&decodedFrame); });

        error = avcodec_receive_frame(m_decoderCtx, decodedFrame);
        // There is not enough data to decode
        if (error == AVERROR(EAGAIN) || error == AVERROR_EOF)
            break;

        if (error != 0)
        {
            NX_WARNING(this, "Could not receive audio frame from decoder, Error code: %1.",
                nx::media::ffmpeg::avErrorToString(error));
            return false;
        }

        return m_encoder.sendFrame(decodedFrame);
    }
    return true;
}

bool QnFfmpegAudioTranscoder::receivePacket(QnAbstractMediaDataPtr* const result)
{
    QnWritableCompressedAudioDataPtr resultAudio;
    if (!m_encoder.receivePacket(resultAudio))
        return false;

    if (resultAudio)
    {
        resultAudio->channelNumber = m_channelNumber;
        *result = resultAudio;
    }

    return true;
}

void QnFfmpegAudioTranscoder::setSampleRate(int value)
{
    m_encoder.setSampleRate(value);
}
