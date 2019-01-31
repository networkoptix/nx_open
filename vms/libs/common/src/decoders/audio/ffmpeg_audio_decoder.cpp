#include "ffmpeg_audio_decoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <cassert>

#include "nx/streaming/audio_data_packet.h"
#include "audio_struct.h"
#include "utils/media/ffmpeg_helper.h"
#include <nx/utils/log/log_main.h>

struct AVCodecContext;

#define INBUF_SIZE 4096

bool QnFfmpegAudioDecoder::m_first_instance = true;

QnFfmpegAudioDecoder::QnFfmpegAudioDecoder(QnCompressedAudioDataPtr data):
    m_codec(data->compressionType),
    m_outFrame(av_frame_alloc())
{
    if (m_first_instance)
        m_first_instance = false;

    if (m_codec == AV_CODEC_ID_NONE)
        return;

    codec = avcodec_find_decoder(m_codec);

    m_audioDecoderCtx = avcodec_alloc_context3(codec);

    if (data->context)
        QnFfmpegHelper::mediaContextToAvCodecContext(m_audioDecoderCtx, data->context);
    else
        NX_ASSERT(false, Q_FUNC_INFO, "Audio packets without codec is deprecated!");

    m_initialized = avcodec_open2(m_audioDecoderCtx, codec, nullptr) >= 0;

    if (m_audioDecoderCtx && !m_initialized)
    {
        const char* codecName = avcodec_get_name(m_codec);
        NX_WARNING(this, lm("Can't create audio decoder for codec %1").arg(codecName));
    }
}

bool QnFfmpegAudioDecoder::isInitialized() const
{
    return m_initialized;
}

QnFfmpegAudioDecoder::~QnFfmpegAudioDecoder(void)
{
    QnFfmpegHelper::deleteAvCodecContext(m_audioDecoderCtx);
    av_frame_free(&m_outFrame);
}

// The input buffer must be FF_INPUT_BUFFER_PADDING_SIZE larger than the actual read bytes because
// some optimized bit stream readers read 32 or 64 bits at once and could read over the end.
// The end of the input buffer buf should be set to 0 to ensure that no over reading happens for
// damaged MPEG streams.
bool QnFfmpegAudioDecoder::decode(QnCompressedAudioDataPtr& data, QnByteArray& result)
{
    result.clear();

    if (!codec)
        return false;

    const unsigned char* inbuf_ptr = (const unsigned char*) data->data();
    int size = static_cast<int>(data->dataSize());
    unsigned char* outbuf = (unsigned char*)result.data();

    int outbuf_len = 0;

    while (size > 0)
    {
        QnFfmpegAvPacket avpkt((quint8*) inbuf_ptr, size);
        int got_frame = 0;
        // TODO: ffmpeg-test.
        // TODO: #dmishin Get rid of deprecated functions.
        int inputConsumed = avcodec_decode_audio4(m_audioDecoderCtx, m_outFrame, &got_frame, &avpkt);
        if (inputConsumed < 0)
            return false;
        if (got_frame)
        {
            int decodedBytes =
                m_outFrame->nb_samples * QnFfmpegHelper::audioSampleSize(m_audioDecoderCtx);
            if (outbuf_len + decodedBytes > (int)result.capacity())
            {
                result.reserve(result.capacity() * 2);
                outbuf = (quint8*)result.data() + outbuf_len;
            }

            if (!m_audioHelper)
                m_audioHelper.reset(new QnFfmpegAudioHelper(m_audioDecoderCtx));
            m_audioHelper->copyAudioSamples(outbuf, m_outFrame);

            outbuf_len += decodedBytes;
            outbuf += decodedBytes;
        }

        size -= inputConsumed;
        inbuf_ptr += inputConsumed;

    }

    result.finishWriting(static_cast<unsigned int>(outbuf_len));
    return true;
}

#endif // ENABLE_DATA_PROVIDERS
