#include "avi_strem_reader.h"
#include "device\device.h"
#include "avi_parser.h"

#include "libavformat/avformat.h"
#include "data/mediadata.h"
#include "stdint.h"




CLAVIStreamReader::CLAVIStreamReader(CLDevice* dev ):
CLAbstractArchiveReader(dev),
m_videoStrmIndex(-1),
mFirstTime(true)
{
	//init();
}

CLAVIStreamReader::~CLAVIStreamReader()
{
	destroy();
}


unsigned long CLAVIStreamReader::currTime() const
{
	QMutexLocker mutex(&m_cs);
	return cuur_time;
}

bool CLAVIStreamReader::init()
{
	static bool firstInstance = true;

	QMutexLocker mutex(&m_cs);

	if (firstInstance)
	{
		firstInstance = false;
		avidll.av_register_all();
	}

	cuur_time = 0;
	m_need_tosleep = 0;

	m_formatContext = avidll.avformat_alloc_context();


	int err = avidll.av_open_input_file(&m_formatContext, m_device->getUniqueId().toLatin1().data(), NULL, 0, NULL);
	if (err < 0)
	{
		destroy();
		return false; 
	}

	err = avidll.av_find_stream_info(m_formatContext);
	if (err < 0)
	{
		destroy();
		return false; 
	}

	m_len_msec = m_formatContext->duration/1000;

	for(unsigned i = 0; i < m_formatContext->nb_streams; i++) 
	{
		AVStream *strm= m_formatContext->streams[i];
		AVCodecContext *codecContext = strm->codec;

		if(codecContext->codec_type >= (unsigned)AVMEDIA_TYPE_NB)
			continue;

		switch(codecContext->codec_type) 
		{
		case AVMEDIA_TYPE_VIDEO:
			if (m_videoStrmIndex == -1) // Take only first video stream
				m_videoStrmIndex = i;
			break;
		default:
			break;
		}
	}


	int ffmpeg_codec_id = m_formatContext->streams[m_videoStrmIndex]->codec->codec_id;

	switch(ffmpeg_codec_id)
	{
	case CODEC_ID_MSVIDEO1:
		m_codec_id =CL_MSVIDEO1;
		break;


	case CODEC_ID_MJPEG:
		m_codec_id =CL_JPEG;
		break;

	case CODEC_ID_MPEG2VIDEO:
		m_codec_id = CL_MPEG2;
		break;


	case CODEC_ID_MPEG4:
		m_codec_id = CL_MPEG4;
		break;

	case CODEC_ID_H264:
		m_codec_id = CL_H264;
		break;
	default:
		destroy();
		return false;
	}


	// Alloc common resources

	avidll.av_init_packet(&m_packet);
	return true;
}


CLAbstractMediaData* CLAVIStreamReader::getNextData()
{
	if (mFirstTime)
	{
		init();
		mFirstTime = false;
	}

	if (!m_formatContext || m_videoStrmIndex==-1)
		return 0;

	if (!isSingleShotMode())	
		mAdaptiveSleep.sleep(m_need_tosleep);
	


	{
		QMutexLocker mutex(&m_cs);

		if (!getNextVideoPacket())
			return 0;

		unsigned long duration = m_formatContext->streams[m_videoStrmIndex]->duration;
		if (duration==0)
			duration = 1;

		cuur_time =  qreal(m_len_msec)*m_packet.dts/duration;
		m_need_tosleep = m_len_msec/duration;

		int t = m_len_msec/m_formatContext->streams[m_videoStrmIndex]->nb_frames;
		t=t;
	}

	
	//int extra = FF_INPUT_BUFFER_PADDING_SIZE;
	int extra = 0;

	CLCompressedVideoData* videoData = new CLCompressedVideoData(0,m_packet.size + extra);
	CLByteArray& data = videoData->data;

	data.prepareToWrite(m_packet.size + extra);
	memcpy(data.data(), m_packet.data, m_packet.size);
	memset(data.data() + m_packet.size, 0, extra);
	data.done(m_packet.size + extra);

	/**/
	FILE* f = fopen("test.264_", "wb");
	fwrite(data.data(), 1, data.size(), f);
	fclose(f);
	/**/


	videoData->compressionType = m_codec_id;
	videoData->keyFrame = 1;//m_formatContext->streams[m_videoStrmIndex]->codec->coded_frame->key_frame;
	videoData->channel_num = 0;


	if (videoData->keyFrame)
		m_gotKeyFrame[0] = true;

	//=================
	if (isSingleShotMode())
		pause();

	

	avidll.av_free_packet(&m_packet);

	return videoData;

}

void CLAVIStreamReader::channeljumpTo(unsigned long msec, int channel)
{
	QMutexLocker mutex(&m_cs);

	int err = avidll.avformat_seek_file(m_formatContext, -1, 0, msec*1000, _I64_MAX, 0);

	if (err < 0) 
	{
		err = err;
	}

}



void CLAVIStreamReader::destroy()
{
	avidll.av_free(m_formatContext);
	m_formatContext = 0;
}


bool CLAVIStreamReader::getNextVideoPacket()
{
	while(1)
	{
		int err = avidll.av_read_frame(m_formatContext, &m_packet);
		if (err < 0)
		{
			if (err == AVERROR_EOF)
			{
				destroy();
				init();

				err = avidll.av_read_frame(m_formatContext, &m_packet);
				if (err<0)
					return false;

			}
			else
				return false;
		}

		if (m_packet.stream_index != m_videoStrmIndex)
		{
			avidll.av_free_packet(&m_packet);
			continue;
		}

		break;

	}

	return true;

}