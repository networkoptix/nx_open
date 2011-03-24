#include "avi_strem_reader.h"
#include "device/device.h"

#include "data/mediadata.h"
#include "stdint.h"

QMutex avi_mutex;

static qint64 packetTimestamp(AVStream* stream, const AVPacket& packet)
{
	double timeBase = av_q2d(stream->time_base);
	qint64 firstDts = (stream->first_dts == AV_NOPTS_VALUE) ? 0 : stream->first_dts;
	double ttm = timeBase * (packet.dts - firstDts);

	return qint64(1e+6 * ttm);
}

extern QMutex global_ffmpeg_mutex;

CLAVIStreamReader::CLAVIStreamReader(CLDevice* dev )
    : CLAbstractArchiveReader(dev),
      m_videoStreamIndex(-1),
      m_audioStreamIndex(-1),
      mFirstTime(true),
      m_formatContext(0),
      m_bsleep(false),
      m_currentPacketIndex(0),
      m_haveSavedPacket(false)
{
    // Should init packets here as some times destroy (av_free_packet) could be called before init
    av_init_packet(&m_packets[0]);
    av_init_packet(&m_packets[1]);

	//init();
}

CLAVIStreamReader::~CLAVIStreamReader()
{
	destroy();
}

void CLAVIStreamReader::previousFrame(quint64 mksec)
{
    jumpToPreviousFrame(mksec, true);
}

void CLAVIStreamReader::switchPacket()
{
    m_currentPacketIndex ^= 1;
}

AVPacket& CLAVIStreamReader::currentPacket()
{
    return m_packets[m_currentPacketIndex];
}

AVPacket& CLAVIStreamReader::nextPacket()
{
    return m_packets[m_currentPacketIndex ^ 1];
}

quint64 CLAVIStreamReader::currentTime() const
{
	QMutexLocker mutex(&m_cs);

    quint64 jumpTime = skipFramesToTime();

	if (jumpTime)
		return jumpTime;

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

        extern URLProtocol ufile_protocol;

        av_register_protocol2(&ufile_protocol, sizeof(ufile_protocol));
	}

	m_currentTime = 0;
	m_previousTime = -1;
	m_needToSleep = 0;

    QString url = "ufile:" + m_device->getUniqueId();
    int err = av_open_input_file(&m_formatContext, url.toUtf8().constData(), NULL, 0, NULL);
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

    if (m_formatContext->start_time != AV_NOPTS_VALUE)
        m_startMksec = m_formatContext->start_time;

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
    av_init_packet(&m_packets[0]);
    av_init_packet(&m_packets[1]);

	return true;
}

CLCompressedVideoData* CLAVIStreamReader::getVideoData(const AVPacket& packet, AVCodecContext* codecContext)
{
    int extra = 0;
    CLCompressedVideoData* videoData = new CLCompressedVideoData(CL_MEDIA_ALIGNMENT, packet.size + extra, codecContext);
    CLByteArray& data = videoData->data;

    data.prepareToWrite(packet.size + extra);
    memcpy(data.data(), packet.data, packet.size);
    memset(data.data() + packet.size, 0, extra);
    data.done(packet.size + extra);

    videoData->compressionType = m_videoCodecId;
    videoData->keyFrame = packet.flags & PKT_FLAG_KEY;
    videoData->channelNumber = 0;
    videoData->timestamp = m_currentTime;

    videoData->useTwice = m_useTwice;
    if (videoData->keyFrame)
        m_gotKeyFrame[0] = true;

    return videoData;
}

CLCompressedAudioData* CLAVIStreamReader::getAudioData(const AVPacket& packet, AVStream* stream)
{
    int extra = 0;
    CLCompressedAudioData* audioData = new CLCompressedAudioData(CL_MEDIA_ALIGNMENT, packet.size + extra, stream->codec);
    CLByteArray& data = audioData->data;

    data.prepareToWrite(packet.size + extra);
    memcpy(data.data(), packet.data, packet.size);
    memset(data.data() + packet.size, 0, extra);
    data.done(packet.size + extra);

    audioData->compressionType = m_audioCodecId;
    audioData->channels = m_channels;

    double time_base = av_q2d(stream->time_base);
    audioData->timestamp = packetTimestamp(stream, packet);
    audioData->duration = qint64(1e+6 * time_base * packet.duration);

    audioData->freq = m_freq;

    return audioData;
}

bool CLAVIStreamReader::getNextVideoPacket()
{
    for (;;)
    {
        // Get next video packet and store it
        if (!getNextPacket(nextPacket()))
            return false;

        if (nextPacket().stream_index == m_videoStreamIndex)
            return true;

        av_free_packet(&nextPacket());
    }
}

CLAbstractMediaData* CLAVIStreamReader::getNextData()
{
	if (mFirstTime)
	{
        QMutexLocker mutex(&avi_mutex); // speeds up concurrent reading of lots of files, if files not cashed yet 
		init(); // this is here instead if constructor to unload ui thread 
		mFirstTime = false;
	}

	if (!m_formatContext || m_videoStreamIndex == -1)
		return 0;

	if (m_bsleep && !isSingleShotMode() && m_needSleep && !isSkippingFrames())
	{
		smartSleep(m_needToSleep);
	}

	{
		QMutexLocker mutex(&m_cs);

        if (m_haveSavedPacket && m_previousTime == -1)
        {
            av_free_packet(&nextPacket());
            m_haveSavedPacket = false;
        }

		// If there is no nextPacket - read it from file, otherwise use saved packet
		if (!m_haveSavedPacket)
		{
			if (!getNextPacket(currentPacket()))
				return 0;
		} else
        {
            switchPacket();
            m_haveSavedPacket = false;
        }

		if (currentPacket().stream_index == m_videoStreamIndex) // in case of video packet 
		{
			m_currentTime =  packetTimestamp(m_formatContext->streams[m_videoStreamIndex], currentPacket());
			if (m_previousTime != -1)
			{
				// we assume that we have constant frame rate 
				m_needToSleep = m_currentTime - m_previousTime;
			}

			m_previousTime = m_currentTime;
		}
	}

	if (currentPacket().stream_index == m_videoStreamIndex) // in case of video packet 
	{
		m_bsleep = true; // sleep only in case of video

		AVCodecContext* codecContext = m_formatContext->streams[m_videoStreamIndex]->codec;
        CLCompressedVideoData* videoData = getVideoData(currentPacket(), codecContext);

		m_useTwice = false;

		if (skipFramesToTime())
		{
			if (!m_haveSavedPacket)
			{
                QMutexLocker mutex(&m_cs);

                if (!getNextVideoPacket())
                {
                    // Some error or end of file. Stop reading frames.
                    setSkipFramesToTime(0);

                    av_free_packet(&nextPacket());
                    m_haveSavedPacket = false;
                }

                m_haveSavedPacket = true;
			}

			if (m_haveSavedPacket)
			{
				quint64 nextPacketTimestamp = packetTimestamp(m_formatContext->streams[m_videoStreamIndex], nextPacket());
				if (nextPacketTimestamp < skipFramesToTime())
				{
					videoData->ignore = true;
				}
				else
				{
					setSkipFramesToTime(0);
				}
			}
		}

		//=================
		if (isSingleShotMode() && skipFramesToTime() == 0)
			pause();

		av_free_packet(&currentPacket());
		return videoData;
	}
	else if (currentPacket().stream_index == m_audioStreamIndex) // in case of audio packet 
	{
        AVStream* stream = m_formatContext->streams[m_audioStreamIndex];

		m_bsleep = false;

        CLCompressedAudioData* audioData = getAudioData(currentPacket(), stream);
		av_free_packet(&currentPacket());
		return audioData;
	}

	av_free_packet(&currentPacket());

	return 0;

}

void CLAVIStreamReader::channeljumpTo(quint64 mksec, int /*channel*/)
{
	QMutexLocker mutex(&m_cs);

    mksec += startMksec();

#ifdef _WIN32
	avformat_seek_file(m_formatContext, -1, 0, mksec, _I64_MAX, AVSEEK_FLAG_BACKWARD);
#else
	avformat_seek_file(m_formatContext, -1, 0, mksec, LLONG_MAX, AVSEEK_FLAG_BACKWARD);
#endif

	m_needToSleep = 0;
	m_previousTime = -1;
	m_wakeup = true;
}

void CLAVIStreamReader::destroy()
{
	if (m_formatContext) // crashes without condition 
    {
		av_close_input_file(m_formatContext);
        m_formatContext = 0;
    }

    av_free_packet(&nextPacket());
}

bool CLAVIStreamReader::getNextPacket(AVPacket& packet)
{
	while (1)
	{
		int err = av_read_frame(m_formatContext, &packet);
		if (err < 0)
		{
			destroy();
			init();

			err = av_read_frame(m_formatContext, &packet);
			if (err < 0)
				return false;
		}

		if (packet.stream_index != m_videoStreamIndex && packet.stream_index != m_audioStreamIndex)
		{
			av_free_packet(&packet);
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

