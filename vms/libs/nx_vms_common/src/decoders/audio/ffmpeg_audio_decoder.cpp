// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_audio_decoder.h"

#include <nx/media/audio_data_packet.h>
#include <nx/media/ffmpeg/av_packet.h>
#include <nx/media/ffmpeg/ffmpeg_utils.h>
#include <nx/utils/log/log_main.h>

struct AVCodecContext;

#define INBUF_SIZE 4096

bool QnFfmpegAudioDecoder::m_first_instance = true;

QnFfmpegAudioDecoder::QnFfmpegAudioDecoder(const QnCompressedAudioDataPtr& data):
    m_codec(data->compressionType),
    m_outFrame(av_frame_alloc())
{
    if (m_first_instance)
        m_first_instance = false;

    if (m_codec == AV_CODEC_ID_NONE)
        return;

    auto codec = avcodec_find_decoder(m_codec);

    m_audioDecoderCtx = avcodec_alloc_context3(codec);

    if (data->context)
        data->context->toAvCodecContext(m_audioDecoderCtx);
    else
        NX_ASSERT(false, "Audio packets without codec is deprecated!");

    // MP3 is always decoded into AV_SAMPLE_FMT_FLTP (planar float), but this format isn't accessible to plugins
    // created using the camera SDK. So we just work it around here.
    if (m_codec == AV_CODEC_ID_MP3)
        data->context->getAvCodecParameters()->format = AV_SAMPLE_FMT_FLTP;

    m_initialized = avcodec_open2(m_audioDecoderCtx, codec, nullptr) >= 0;

    if (m_audioDecoderCtx && !m_initialized)
    {
        const char* codecName = avcodec_get_name(m_codec);
        NX_WARNING(this, "Can't create audio decoder for codec %1", codecName);
    }
}

bool QnFfmpegAudioDecoder::isInitialized() const
{
    return m_initialized;
}

QnFfmpegAudioDecoder::~QnFfmpegAudioDecoder(void)
{
    avcodec_free_context(&m_audioDecoderCtx);
    av_frame_free(&m_outFrame);
}

// The input buffer must be AV_INPUT_BUFFER_PADDING_SIZE larger than the actual read bytes because
// some optimized bit stream readers read 32 or 64 bits at once and could read over the end.
// The end of the input buffer buf should be set to 0 to ensure that no over reading happens for
// damaged MPEG streams.
bool QnFfmpegAudioDecoder::decode(QnCompressedAudioDataPtr& data, nx::utils::ByteArray& result)
{
    result.clear();

    if (!m_audioDecoderCtx)
        return false;

    const unsigned char* inbuf_ptr = (const unsigned char*) data->data();
    int size = static_cast<int>(data->dataSize());
    unsigned char* outbuf = (unsigned char*)result.data();

    nx::media::ffmpeg::AvPacket avpkt((quint8*) inbuf_ptr, size);
    int error = avcodec_send_packet(m_audioDecoderCtx, avpkt.get());
    if (error < 0)
    {
        NX_WARNING(this, "Failed to decode audio packet timestamp: %1, error: %2", data->timestamp,
            nx::media::ffmpeg::avErrorToString(error));
        return false;
    }

    int outbuf_len = 0;
    while (error >= 0)
    {
        error = avcodec_receive_frame(m_audioDecoderCtx, m_outFrame);
        if (error == AVERROR(EAGAIN) || error == AVERROR_EOF)
            break;

        if (error < 0)
        {
            NX_WARNING(this, "Failed to decode(receive) audio packet timestamp: %1, error: %2",
                data->timestamp,
                nx::media::ffmpeg::avErrorToString(error));
            return false;
        }

        int decodedBytes =
            m_outFrame->nb_samples * QnFfmpegHelper::audioSampleSize(m_audioDecoderCtx);
        if (outbuf_len + decodedBytes > (int)result.capacity())
        {
            result.reserve(result.capacity() * 2);
            outbuf = (quint8*)result.data() + outbuf_len;
        }

        if (!m_audioHelper || !m_audioHelper->isCompatible(m_audioDecoderCtx))
            m_audioHelper.reset(new QnFfmpegAudioHelper(m_audioDecoderCtx));
        m_audioHelper->copyAudioSamples(outbuf, m_outFrame);

        outbuf_len += decodedBytes;
        outbuf += decodedBytes;
    }
    result.finishWriting(static_cast<unsigned int>(outbuf_len));
    return true;
}
