#include "ffmpeg_audio.h"

#include "core/datapacket/mediadatapacket.h"
#include "audio_struct.h"
#include "utils/common/aligned_data.h"

struct AVCodecContext;

#define INBUF_SIZE 4096

extern QMutex global_ffmpeg_mutex;
extern int MAX_AUDIO_FRAME_SIZE;

bool CLFFmpegAudioDecoder::m_first_instance = true;

AVSampleFormat CLFFmpegAudioDecoder::audioFormatQtToFfmpeg(const QnAudioFormat& fmt)
{

    //int s = fmt.sampleSize();
    //QAudioFormat::SampleType st = fmt.sampleType();
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

//================================================

CLFFmpegAudioDecoder::CLFFmpegAudioDecoder(QnCompressedAudioDataPtr data):
    c(0),
    m_codec(data->compressionType)
{

	QMutexLocker mutex(&global_ffmpeg_mutex);

	if (m_first_instance)
	{
		m_first_instance = false;

	}

//    CodecID codecId = internalCodecIdToFfmpeg(m_codec);

    if (m_codec != CODEC_ID_NONE)
    {
        codec = avcodec_find_decoder(m_codec);
    }
    else
    {
        codec = 0;
        c = 0;
        return;
    }

	c = avcodec_alloc_context();

    if (data->context)
    {
        avcodec_copy_context(c, data->context->ctx());
    }
    else {
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
    }
	avcodec_open(c, codec);

}

CLFFmpegAudioDecoder::~CLFFmpegAudioDecoder(void)
{
	QMutexLocker mutex(&global_ffmpeg_mutex);

	if (c)
	{
        if (c->codec)
            avcodec_close(c);
		av_free(c);
	}

}

//The input buffer must be FF_INPUT_BUFFER_PADDING_SIZE larger than the actual read bytes because some optimized bit stream readers read 32 or 64 bits at once and could read over the end.
//The end of the input buffer buf should be set to 0 to ensure that no over reading happens for damaged MPEG streams.
bool CLFFmpegAudioDecoder::decode(QnCompressedAudioDataPtr& data, CLByteArray& result)
{
    result.clear();

	if (!codec)
		return false;

	const unsigned char* inbuf_ptr = (const unsigned char*) data->data.data();
	int size = data->data.size();
	unsigned char* outbuf = (unsigned char*)result.data();

	int outbuf_len = 0;

	while (size > 0) 
	{

		int out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;

		//cl_log.log("before dec",  cl_logALWAYS);

		if (outbuf_len + out_size > result.capacity())
		{
            assert(false);
			outbuf_len = 0;
			outbuf = (unsigned char*)result.data(); // start form beginning
		}

        AVPacket avpkt;
        av_init_packet(&avpkt);
        avpkt.data = (quint8*)inbuf_ptr;
        avpkt.size = size;

        int len = avcodec_decode_audio3(c, (short *)outbuf, &out_size, &avpkt);

		//cl_log.log("after dec",  cl_logALWAYS);

		if (len < 0) 
			return false;

		outbuf_len+=out_size;
		outbuf+=out_size;
		size -= len;
		inbuf_ptr += len;

	}
    result.done(outbuf_len);
	return true;
}
