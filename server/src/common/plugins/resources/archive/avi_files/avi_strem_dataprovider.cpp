#include "avi_strem_dataprovider.h"
#include "common/bytearray.h"
#include "resource/resource.h"
#include "common/base.h"
#include "datapacket/mediadatapacket.h"


QMutex global_ffmpeg_mutex;

QMutex QnPlAVIStreamProvider::avi_mutex;
QSemaphore QnPlAVIStreamProvider::aviSemaphore(4);

class QnAviSemaphoreHelpr
{
public:
    QnAviSemaphoreHelpr(QSemaphore& sem):
      m_sem(sem)
      {
          m_sem.tryAcquire();
      }

    ~QnAviSemaphoreHelpr()
    {
        m_sem.release();
    }

private:
    QSemaphore& m_sem;

};



extern QMutex global_ffmpeg_mutex;
static const int FFMPEG_PROBE_BUFFER_SIZE = 1024 * 512;
static const int MAX_VALID_SLEEP_TIME = 1000 * 1000 * 5; // 5 seconds as most long sleep time

qint64 QnPlAVIStreamProvider::packetTimestamp(AVStream* stream, const AVPacket& packet)
{
	double timeBase = av_q2d(stream->time_base);
	qint64 firstDts = (stream->first_dts == AV_NOPTS_VALUE) ? 0 : stream->first_dts;
	double ttm = timeBase * (packet.dts - firstDts);

	return qint64(1e+6 * ttm);
}

QnPlAVIStreamProvider::QnPlAVIStreamProvider(QnResource* dev )
    : QnPlAbstractArchiveProvider(dev),
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

QnPlAVIStreamProvider::~QnPlAVIStreamProvider()
{
	destroy();
}

void QnPlAVIStreamProvider::previousFrame(quint64 mksec)
{
    jumpToPreviousFrame(mksec, true);
}

void QnPlAVIStreamProvider::switchPacket()
{
    m_currentPacketIndex ^= 1;
}

AVPacket& QnPlAVIStreamProvider::currentPacket()
{
    return m_packets[m_currentPacketIndex];
}

AVPacket& QnPlAVIStreamProvider::nextPacket()
{
    return m_packets[m_currentPacketIndex ^ 1];
}

quint64 QnPlAVIStreamProvider::currentTime() const
{
	QMutexLocker mutex(&m_cs);

    quint64 jumpTime = skipFramesToTime();

	if (jumpTime)
		return jumpTime;

	return m_currentTime;
}

ByteIOContext* QnPlAVIStreamProvider::getIOContext() 
{
    return 0;
}

AVFormatContext* QnPlAVIStreamProvider::getFormatContext()
{
    QString url = "ufile:" + m_device->getUniqueId();
    AVFormatContext* formatContext;
    int err = av_open_input_file(&formatContext, url.toUtf8().constData(), NULL, 0, NULL);
    if (err < 0)
    {
        destroy();
        return 0; 
    }

    {
        QMutexLocker global_ffmpeg_locker(&global_ffmpeg_mutex);
        err = av_find_stream_info(formatContext);
        if (err < 0)
        {
            destroy();
            return 0;
        }
    }
    return formatContext;
}

bool QnPlAVIStreamProvider::init()
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
    m_lengthMksec = 0;

    m_formatContext = getFormatContext();
    if (!m_formatContext)
        return false;

    if (m_lengthMksec == 0)
	    m_lengthMksec = m_formatContext->duration; // it is not filled during opening context

    if (m_formatContext->start_time != AV_NOPTS_VALUE)
        m_startMksec = m_formatContext->start_time;
    
    if (!initCodecs())
        return false;

    // Alloc common resources
    av_init_packet(&m_packets[0]);
    av_init_packet(&m_packets[1]);

	return true;
}

bool QnPlAVIStreamProvider::initCodecs()
{
    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;

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

    if (m_videoStreamIndex != -1)
    {
        CodecID ffmpeg_video_codec_id = m_formatContext->streams[m_videoStreamIndex]->codec->codec_id;

        if (ffmpeg_video_codec_id != CODEC_ID_NONE)
        {
            m_videoCodecId = ffmpeg_video_codec_id;
        }
        else
        {
            destroy();
            return false;
        }

    }

    if (m_audioStreamIndex!=-1)
    {

        AVCodecContext *aCodecCtx = m_formatContext->streams[m_audioStreamIndex]->codec;
        CodecID ffmpeg_audio_codec_id = aCodecCtx->codec_id;
        m_freq = aCodecCtx->sample_rate;
        m_channels = aCodecCtx->channels;

        m_audioCodecId = ffmpeg_audio_codec_id;

    }
    return true;
}

QnCompressedVideoDataPtr QnPlAVIStreamProvider::getVideoData(const AVPacket& packet, AVCodecContext* codecContext)
{
    int extra = 0;
    QnCompressedVideoDataPtr videoData ( new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, packet.size + extra, codecContext) );
    CLByteArray& data = videoData->data;

    data.prepareToWrite(packet.size + extra);
    memcpy(data.data(), packet.data, packet.size);
    memset(data.data() + packet.size, 0, extra);
    data.done(packet.size + extra);

    videoData->compressionType = m_videoCodecId;
    videoData->keyFrame = packet.flags & AV_PKT_FLAG_KEY;
    videoData->channelNumber = 0;
    videoData->timestamp = m_currentTime;

    videoData->useTwice = m_useTwice;
    if (videoData->keyFrame)
        m_gotKeyFrame[0] = true;

    return videoData;
}

QnCompressedAudioDataPtr QnPlAVIStreamProvider::getAudioData(const AVPacket& packet, AVStream* stream)
{
    int extra = 0;
    QnCompressedAudioDataPtr audioData ( new QnCompressedAudioData(CL_MEDIA_ALIGNMENT, packet.size + extra, stream->codec) );
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

bool QnPlAVIStreamProvider::getNextVideoPacket()
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

QnAbstractMediaDataPacketPtr QnPlAVIStreamProvider::getNextData()
{
	if (mFirstTime)
	{
        QMutexLocker mutex(&avi_mutex); // speeds up concurrent reading of lots of files, if files not cashed yet 
		init(); // this is here instead if constructor to unload ui thread 
		mFirstTime = false;
	}

    QnAviSemaphoreHelpr sem(aviSemaphore);

    if (!m_formatContext || m_videoStreamIndex == -1)
		return QnAbstractMediaDataPacketPtr(0);

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
				return QnAbstractMediaDataPacketPtr(0);
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
                if(m_needToSleep > MAX_VALID_SLEEP_TIME)
                {
                    AVRational frameRate = m_formatContext->streams[m_videoStreamIndex]->r_frame_rate;
                    if (frameRate.den && frameRate.num)
                    {
                        qint64 frameDuration = 1000000ull * frameRate.den / frameRate.num;
                        if (frameDuration)
                        {
                            // it is a jump between PTS, so use default sleep value
                            m_needToSleep = frameDuration;
                        }
                    }
                }
			}

			m_previousTime = m_currentTime;
		}
	}

	if (currentPacket().stream_index == m_videoStreamIndex) // in case of video packet 
	{
		m_bsleep = true; // sleep only in case of video

		AVCodecContext* codecContext = m_formatContext->streams[m_videoStreamIndex]->codec;
        QnCompressedVideoDataPtr videoData ( getVideoData(currentPacket(), codecContext) );

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

        QnCompressedAudioDataPtr audioData ( getAudioData(currentPacket(), stream) );
		av_free_packet(&currentPacket());
		return audioData;
	}

	av_free_packet(&currentPacket());

	return QnAbstractMediaDataPacketPtr(0);

}

void QnPlAVIStreamProvider::channeljumpTo(quint64 mksec, int /*channel*/)
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

void QnPlAVIStreamProvider::destroy()
{
	if (m_formatContext) // crashes without condition 
    {
        av_close_input_file(m_formatContext);
        m_formatContext = 0;
    }

    av_free_packet(&nextPacket());
}

bool QnPlAVIStreamProvider::getNextPacket(AVPacket& packet)
{
	while (1)
	{
		int err = av_read_frame(m_formatContext, &packet);
		if (err < 0)
		{
            
			destroy();
			if (!init())
                return false;
            
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

void QnPlAVIStreamProvider::smartSleep(qint64 mksec)
{
    if (mksec < 0)
        return;
    // 45000 more that wide used fps values in movies, so we are going to sleep only once for AVI movies (it is more carefull than 2 slepp calls)
    static const int SLEEP_WAKEUP_INTERVAL = 45000; 
	m_wakeup = false;
	int sleep_times = mksec/SLEEP_WAKEUP_INTERVAL; // 40000, 1

	for (int i = 0; i < sleep_times; ++i)
	{
		if (m_wakeup || needToStop())
			return;

		m_adaptiveSleep.sleep(SLEEP_WAKEUP_INTERVAL);
	}

	if (m_wakeup || needToStop())
		return;

	m_adaptiveSleep.sleep(mksec%SLEEP_WAKEUP_INTERVAL);

}

