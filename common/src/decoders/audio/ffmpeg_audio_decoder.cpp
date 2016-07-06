#include "ffmpeg_audio_decoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <cassert>

#include "nx/streaming/audio_data_packet.h"
#include "audio_struct.h"
#include "utils/media/ffmpeg_helper.h"

struct AVCodecContext;

#define INBUF_SIZE 4096

bool QnFfmpegAudioDecoder::m_first_instance = true;

AVSampleFormat QnFfmpegAudioDecoder::audioFormatQtToFfmpeg(const QnAudioFormat& fmt)
{
    if (fmt.sampleSize() == 8)
        return AV_SAMPLE_FMT_U8;
    else if(fmt.sampleSize() == 16 && fmt.sampleType() == QnAudioFormat::SignedInt)
        return AV_SAMPLE_FMT_S16;
    else if(fmt.sampleSize() == 32 && fmt.sampleType() == QnAudioFormat::SignedInt)
        return AV_SAMPLE_FMT_S32;
    else if(fmt.sampleSize() == 32 && fmt.sampleType() == QnAudioFormat::Float)
        return AV_SAMPLE_FMT_FLT;
    else
        return AV_SAMPLE_FMT_NONE;
}

// ================================================

QnFfmpegAudioDecoder::QnFfmpegAudioDecoder(QnCompressedAudioDataPtr data):
    c(0),
    m_codec(data->compressionType),
    m_outFrame(av_frame_alloc())
{
    if (m_first_instance)
    {
        m_first_instance = false;
    }

//    AVCodecID codecId = internalCodecIdToFfmpeg(m_codec);

    if (m_codec != AV_CODEC_ID_NONE)
    {
        codec = avcodec_find_decoder(m_codec);
    }
    else
    {
        codec = 0;
        c = 0;
        return;
    }

    c = avcodec_alloc_context3(codec);

    if (data->context)
    {
        QnFfmpegHelper::mediaContextToAvCodecContext(c, data->context);
    }
    else {
        NX_ASSERT(false, Q_FUNC_INFO, "Audio packets without codec is deprecated!");
        /*
        c->codec_id = m_codec;
        c->codec_type = AVMEDIA_TYPE_AUDIO;
        c->sample_fmt = audioFormatQtToFfmpeg(data->format);
        c->channels = data->format.channels();
        c->sample_rate = data->format.frequency();
        c->bits_per_coded_sample = qMin(24, data->format.sampleSize());

        c->bit_rate = data->format.bitrate;
        c->block_align = data->format.block_align;
        c->channel_layout = data->format.channel_layout;

        if (data->format.extraData.size() > 0)
        {
            c->extradata_size = data->format.extraData.size();
            c->extradata = (quint8*) av_malloc(c->extradata_size);
            memcpy(c->extradata, &data->format.extraData[0], c->extradata_size);
        }
        */
    }
    avcodec_open2(c, codec, NULL);
}

QnFfmpegAudioDecoder::~QnFfmpegAudioDecoder(void)
{
    QnFfmpegHelper::deleteAvCodecContext(c);
    c = 0;
    av_frame_free(&m_outFrame);
}

//The input buffer must be FF_INPUT_BUFFER_PADDING_SIZE larger than the actual read bytes because some optimized bit stream readers read 32 or 64 bits at once and could read over the end.
//The end of the input buffer buf should be set to 0 to ensure that no over reading happens for damaged MPEG streams.
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

        AVPacket avpkt;
        av_init_packet(&avpkt);
        avpkt.data = (quint8*)inbuf_ptr;
        avpkt.size = size;

        //int len = avcodec_decode_audio3(c, (short *)outbuf, &out_size, &avpkt);

        int got_frame = 0;
        // todo: ffmpeg-test
        int len = avcodec_decode_audio4(c, m_outFrame, &got_frame, &avpkt);
        if (got_frame)
        {
            if (outbuf_len + m_outFrame->pkt_size > (int)result.capacity())
            {
                //NX_ASSERT(false, Q_FUNC_INFO, "Too small output buffer for audio decoding!");
                result.reserve(result.capacity() * 2);
                outbuf = (quint8*)result.data() + outbuf_len;
            }

            memcpy(outbuf, m_outFrame->data, m_outFrame->pkt_size);
        }

        if (len < 0)
            return false;

        outbuf_len += m_outFrame->pkt_size;
        outbuf += m_outFrame->pkt_size;
        size -= len;
        inbuf_ptr += len;

    }
    result.finishWriting(outbuf_len);
    return true;
}

#endif // ENABLE_DATA_PROVIDERS
