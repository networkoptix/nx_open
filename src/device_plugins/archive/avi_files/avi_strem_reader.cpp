#include "avi_strem_reader.h"
#include "device\device.h"

#include "data/mediadata.h"
#include "stdint.h"

extern QMutex global_ffmpeg_mutex;

CLAVIStreamReader::CLAVIStreamReader(CLDevice* dev ):
CLAbstractArchiveReader(dev),
m_videoStreamIndex(-1),
m_audioStreamIndex(-1),
mFirstTime(true),
m_formatContext(0),
m_bsleep(false)
{
	//init();
}

CLAVIStreamReader::~CLAVIStreamReader()
{
	destroy();
}

quint64 CLAVIStreamReader::currentTime() const
{
	QMutexLocker mutex(&m_cs);
	return m_currentTime;
}

bool CLAVIStreamReader::init()
{
	static bool firstInstance = true;

	QMutexLocker mutex(&m_cs);

	if (firstInstance)
	{
		firstInstance = false;

		QMutexLocker global_ffmpeg_locker(&global_ffmpeg_mutex);
		av_register_all();
	}

	m_currentTime = 0;
	m_previousTime = -1;
	m_needToSleep = 0;

	m_formatContext = avformat_alloc_context();

	int err = av_open_input_file(&m_formatContext, m_device->getUniqueId().toLatin1().data(), NULL, 0, NULL);
	if (err < 0)
	{
		destroy();
		return false; 
	}

	{
		QMutexLocker global_ffmpeg_locker(&global_ffmpeg_mutex);

		err = av_find_stream_info(m_formatContext);
		if (err < 0)
		{
			destroy();
			return false;
		}
	}

	m_lengthMksec = m_formatContext->duration;

	for(unsigned i = 0; i < m_formatContext->nb_streams; i++) 
	{
		AVStream *strm= m_formatContext->streams[i];
		AVCodecContext *codecContext = strm->codec;

		if(codecContext->codec_type >= (unsigned)AVMEDIA_TYPE_NB)
			continue;

		switch(codecContext->codec_type) 
		{
		case AVMEDIA_TYPE_VIDEO:
			if (m_videoStreamIndex == -1) // Take only first video stream
				m_videoStreamIndex = i;
			break;

		case AVMEDIA_TYPE_AUDIO:
			if (m_audioStreamIndex == -1) // Take only first audio stream
				m_audioStreamIndex = i;
			break;

		default:
			break;
		}
	}

	CodecID ffmpeg_video_codec_id = m_formatContext->streams[m_videoStreamIndex]->codec->codec_id;

	switch(ffmpeg_video_codec_id)
	{
	case CODEC_ID_MSVIDEO1:
		m_videoCodecId = CL_MSVIDEO1;
		break;

	case CODEC_ID_MJPEG:
		m_videoCodecId =CL_JPEG;
		break;

	case CODEC_ID_MPEG2VIDEO:
		m_videoCodecId = CL_MPEG2;
		break;

	case CODEC_ID_MPEG4:
		m_videoCodecId = CL_MPEG4;
		break;

	case CODEC_ID_MSMPEG4V3:
		m_videoCodecId = CL_MSMPEG4V3;
		break;

	//case CODEC_ID_MPEG1VIDEO:
	//	m_videoCodecId = CL_MPEG1VIDEO;
	//	break;

	case CODEC_ID_MSMPEG4V2:
		m_videoCodecId = CL_MSMPEG4V2;
		break;

	case CODEC_ID_WMV3:
		m_videoCodecId = CL_WMV3;
		break;

	case CODEC_ID_H264:
		m_videoCodecId = CL_H264;
		break;
	default:
		destroy();
		return false;
	}

	if (m_audioStreamIndex!=-1)
	{

		AVCodecContext *aCodecCtx = m_formatContext->streams[m_audioStreamIndex]->codec;
		CodecID ffmpeg_audio_codec_id = aCodecCtx->codec_id;
		m_freq = aCodecCtx->sample_rate;
		m_channels = aCodecCtx->channels;

		switch(ffmpeg_audio_codec_id )
		{
		case CODEC_ID_MP2:
			m_audioCodecId = CL_MP2;
			break;

		case CODEC_ID_MP3:
			m_audioCodecId = CL_MP3;
			break;

		case CODEC_ID_AC3:
			m_audioCodecId = CL_AC3;
			break;

		case CODEC_ID_AAC:
			m_audioCodecId = CL_AAC;  // crashes 
			break;

		case CODEC_ID_WMAV2:
			m_audioCodecId = CL_WMAV2;
			break;

		case CODEC_ID_WMAPRO:
			m_audioCodecId = CL_WMAPRO;
			break;

		case CODEC_ID_ADPCM_MS:
			m_audioCodecId = CL_ADPCM_MS;
			break;

		case CODEC_ID_AMR_NB:
			m_audioCodecId = CL_AMR_NB;
			break;

		default:
			m_audioCodecId = CL_UNKNOWN;
			break;
		}
	}

	// Alloc common resources
	av_init_packet(&m_packet);
	return true;
}

CLAbstractMediaData* CLAVIStreamReader::getNextData()
{
	if (mFirstTime)
	{
		init();
		mFirstTime = false;
	}

	if (!m_formatContext || m_videoStreamIndex==-1)
		return 0;

	if (m_bsleep && !isSingleShotMode() && m_needSleep)
	{
		smartSleep(m_needToSleep);
	}

	{
		QMutexLocker mutex(&m_cs);

		if (!getNextPacket())
			return 0;

		if (m_packet.stream_index == m_videoStreamIndex) // in case of video packet 
		{
            double timeBase = av_q2d(m_formatContext->streams[m_videoStreamIndex]->time_base);
            qint64 firstDts = (m_formatContext->streams[m_videoStreamIndex]->first_dts == AV_NOPTS_VALUE) ? 0 : m_formatContext->streams[m_videoStreamIndex]->first_dts;
            double ttm = timeBase * (m_packet.dts - firstDts);

			m_currentTime =  qint64(1e+6 * ttm);

			// cl_log.log("ST: " + scurtime + ", " + sall, cl_logALWAYS);
			//m_need_tosleep = m_len_msec/duration;

			//if (m_need_tosleep==0 && m_prev_time!=-1)
			if (m_previousTime != -1)
			{
				// we assume that we have constant frame rate 
				m_needToSleep = m_currentTime-m_previousTime;

			}

			m_previousTime = m_currentTime;
		}

	}

	if (m_packet.stream_index == m_videoStreamIndex) // in case of video packet 
	{
		m_bsleep = true; // sleep only in case of video

		AVCodecContext* codecContext = m_formatContext->streams[m_videoStreamIndex]->codec;
		//int extra = FF_INPUT_BUFFER_PADDING_SIZE;
		int extra = 0;
		CLCompressedVideoData* videoData = new CLCompressedVideoData(CL_MEDIA_ALIGNMENT,m_packet.size + extra, codecContext);
		CLByteArray& data = videoData->data;

		data.prepareToWrite(m_packet.size + extra);
		memcpy(data.data(), m_packet.data, m_packet.size);
		memset(data.data() + m_packet.size, 0, extra);
		data.done(m_packet.size + extra);

		/*	
		FILE* f = fopen("test.mp3", "ab");
		fwrite(data.data(), 1, data.size(), f);
		fclose(f);
		/**/

		videoData->compressionType = m_videoCodecId;
		videoData->keyFrame = m_packet.flags & PKT_FLAG_KEY;
		videoData->channelNumber = 0;
		videoData->timestamp = m_currentTime;

		videoData->useTwice = m_useTwice;
		m_useTwice = false;

		if (videoData->keyFrame)
			m_gotKeyFrame[0] = true;

		//=================
		if (isSingleShotMode())
			pause();

		av_free_packet(&m_packet);
		return videoData;

	}

	else if (m_packet.stream_index == m_audioStreamIndex) // in case of audio packet 
	{
		AVCodecContext* codecContext = m_formatContext->streams[m_audioStreamIndex]->codec;

		m_bsleep = false;

		int extra = 0;
		CLCompressedAudioData* audioData = new CLCompressedAudioData(CL_MEDIA_ALIGNMENT,m_packet.size + extra, codecContext);
		double time_base = av_q2d(m_formatContext->streams[m_audioStreamIndex]->time_base);
        qint64 firstDts = (m_formatContext->streams[m_audioStreamIndex]->first_dts == AV_NOPTS_VALUE) ? 0 : m_formatContext->streams[m_audioStreamIndex]->first_dts;
		double ttm = time_base * (m_packet.dts - firstDts);
		audioData->timestamp = qint64(1e+6 * ttm);
        audioData->duration = qint64(1e+6 * time_base * m_packet.duration);

		CLByteArray& data = audioData->data;

		data.prepareToWrite(m_packet.size + extra);
		memcpy(data.data(), m_packet.data, m_packet.size);
		memset(data.data() + m_packet.size, 0, extra);
		data.done(m_packet.size + extra);

		//FILE* f = fopen("test.mp3", "ab");
		//fwrite(data.data(), 1, data.size(), f);
		//fclose(f);

		audioData->compressionType = m_audioCodecId;
		audioData->channels = m_channels;

		audioData->freq = m_freq;

		//cl_log.log("avi parser: au ps = ", (int)audioData->data.size(), cl_logALWAYS);

		av_free_packet(&m_packet);
		return audioData;
	}
	/**/

	av_free_packet(&m_packet);

	return 0;

}

void CLAVIStreamReader::channeljumpTo(quint64 mksec, int channel)
{
	QMutexLocker mutex(&m_cs);

	int err = avformat_seek_file(m_formatContext, -1, 0, mksec, _I64_MAX, AVSEEK_FLAG_BACKWARD);

	m_needToSleep = 0;
	m_previousTime = -1;
	m_wakeup = true;
}

void CLAVIStreamReader::destroy()
{
	if (m_formatContext) // crashes without condition 
		av_close_input_file(m_formatContext);

	m_formatContext = 0;
}

bool CLAVIStreamReader::getNextPacket()
{
	while(1)
	{
		int err = av_read_frame(m_formatContext, &m_packet);
		if (err < 0)
		{
			//if (err == AVERROR_EOF)
			{
				destroy();
				init();

				err = av_read_frame(m_formatContext, &m_packet);
				if (err<0)
					return false;

			}
			//else
			//	return false;
		}

		if (m_packet.stream_index != m_videoStreamIndex && m_packet.stream_index != m_audioStreamIndex)
		{
			av_free_packet(&m_packet);
			continue;
		}

		break;

	}

	return true;
}

void CLAVIStreamReader::smartSleep(quint64 mksec)
{
	m_wakeup = false;
	int sleep_times = mksec/32000;

	for (int i = 0; i < sleep_times; ++i)
	{
		if (m_wakeup || needToStop())
			return;

		m_adaptiveSleep.sleep(32000);
	}

	if (m_wakeup || needToStop())
		return;

	m_adaptiveSleep.sleep(mksec%32000);

}

