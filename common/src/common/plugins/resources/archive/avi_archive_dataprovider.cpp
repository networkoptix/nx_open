#include "avi_archive_dataprovider.h"
#include "resource/url_resource.h"
#include "datapacket/mediadatapacket.h"
#include "common/base.h"
#include "resource/media_resource.h"

extern QMutex global_ffmpeg_mutex;
QMutex QnAviArchiveDataProvider::avi_mutex;
QSemaphore QnAviArchiveDataProvider::aviSemaphore(4);
static const int FFMPEG_PROBE_BUFFER_SIZE = 1024 * 512;

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

// ---------------------------------- QnAviArchiveDataProvider -----------------------

QnAviArchiveDataProvider::QnAviArchiveDataProvider(QnResourcePtr ptr):
    QnNavigatedDataProvider(ptr),
    m_formatContext(0),
    m_videoStreamIndex(-1),
    m_audioStreamIndex(-1),
    mFirstTime(true),
    m_bsleep(false),
    m_currentPacketIndex(0),
    m_eof(false),
    m_haveSavedPacket(false),
    m_selectedAudioChannel(0)
{
    av_init_packet(&m_packets[0]);
    av_init_packet(&m_packets[1]);
}

QnAviArchiveDataProvider::~QnAviArchiveDataProvider()
{
    destroy();
}


AVFormatContext* QnAviArchiveDataProvider::getFormatContext()
{
    QSharedPointer<QnURLResource> mediaResource = qSharedPointerDynamicCast<QnURLResource>(m_resource);
    QString url = QLatin1String("ufile:") + mediaResource->getUrl();
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

void QnAviArchiveDataProvider::destroy()
{
    if (m_formatContext) // crashes without condition
    {
        av_close_input_file(m_formatContext);
        m_formatContext = 0;
    }

    av_free_packet(&nextPacket());
}

bool QnAviArchiveDataProvider::init()
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
    //m_needToSleep = 0;
    //m_lengthMksec = 0;

    m_formatContext = getFormatContext();
    if (!m_formatContext)
        return false;

    m_lengthMksec = contentLength();

    if (m_formatContext->start_time != AV_NOPTS_VALUE)
        m_startMksec = m_formatContext->start_time;

    if (!initCodecs())
        return false;

    // Alloc common resources
    av_init_packet(&m_packets[0]);
    av_init_packet(&m_packets[1]);

    return true;
}

bool QnAviArchiveDataProvider::initCodecs()
{
    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
    int lastStreamID = -1;

    for(unsigned i = 0; i < m_formatContext->nb_streams; i++)
    {
        AVStream *strm= m_formatContext->streams[i];
        AVCodecContext *codecContext = strm->codec;

        if(codecContext->codec_type >= (unsigned)AVMEDIA_TYPE_NB)
            continue;

        if (strm->id && strm->id == lastStreamID)
            continue; // duplicate
        lastStreamID = strm->id;

        switch(codecContext->codec_type)
        {
        case AVMEDIA_TYPE_VIDEO:
            if (m_videoStreamIndex == -1) // Take only first video stream
                m_videoStreamIndex = i;
            break;

        case AVMEDIA_TYPE_AUDIO:
            if (m_audioStreamIndex == -1) // Take only first audio stream
                m_audioStreamIndex = i;
            m_selectedAudioChannel = 0;
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
        //emit videoParamsChanged(m_formatContext->streams[m_videoStreamIndex]->codec);
    }

    if (m_audioStreamIndex!=-1)
    {
        AVCodecContext *aCodecCtx = m_formatContext->streams[m_audioStreamIndex]->codec;
        CodecID ffmpeg_audio_codec_id = aCodecCtx->codec_id;
        m_freq = aCodecCtx->sample_rate;
        m_channels = aCodecCtx->channels;

        m_audioCodecId = ffmpeg_audio_codec_id;
        //emit audioParamsChanged(aCodecCtx);
    }
    //emit realTimeStreamHint(false);
    return true;
}


QnAbstractDataPacketPtr QnAviArchiveDataProvider::getNextData()
{
	if (mFirstTime)
	{
		QMutexLocker mutex(&avi_mutex); // speeds up concurrent reading of lots of files, if files not cashed yet
		init(); // this is here instead if constructor to unload ui thread
		mFirstTime = false;
	}

	QnAviSemaphoreHelpr sem(aviSemaphore);

	if (!m_formatContext || m_videoStreamIndex == -1)
		return QnAbstractDataPacketPtr(0);

	/*
	if (m_bsleep && !isSingleShotMode() && m_needSleep && !isSkippingFrames())
	{
		smartSleep(m_needToSleep);
	}
	*/

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
				return QnAbstractDataPacketPtr(0);
		} else
		{
			switchPacket();
			m_haveSavedPacket = false;
		}

		if (currentPacket().stream_index == m_videoStreamIndex) // in case of video packet
		{
			m_currentTime =  packetTimestamp(m_formatContext->streams[m_videoStreamIndex], currentPacket());
			m_previousTime = m_currentTime;
		}
	}

	QnAbstractMediaDataPacketPtr data;

	if (currentPacket().stream_index == m_videoStreamIndex) // in case of video packet
	{
		m_bsleep = true; // sleep only in case of video

        AVCodecContext* codecContext = m_formatContext->streams[m_videoStreamIndex]->codec;
        QnCompressedVideoDataPtr videoData = getVideoData(currentPacket(), codecContext);

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

		data =  videoData;
	}
	else if (currentPacket().stream_index == m_audioStreamIndex) // in case of audio packet
	{
		AVStream* stream = m_formatContext->streams[m_audioStreamIndex];

		m_bsleep = false;

		QnCompressedAudioDataPtr audioData = getAudioData(currentPacket(), stream);
		data =  audioData;
	}
	if (data && m_eof)
	{
		data->flags |= QnAbstractMediaDataPacket::MediaFlags_AfterEOF;
		m_eof = false;
	}

	av_free_packet(&currentPacket());
	return data;
}

AVPacket& QnAviArchiveDataProvider::currentPacket()
{
    return m_packets[m_currentPacketIndex];
}

AVPacket& QnAviArchiveDataProvider::nextPacket()
{
    return m_packets[m_currentPacketIndex ^ 1];
}

bool QnAviArchiveDataProvider::getNextPacket(AVPacket& packet)
{
    while (1)
    {
        int err = av_read_frame(m_formatContext, &packet);
        if (err < 0)
        {

            destroy();
            m_eof = true;

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

void QnAviArchiveDataProvider::switchPacket()
{
    m_currentPacketIndex ^= 1;
}

qint64 QnAviArchiveDataProvider::packetTimestamp(AVStream* stream, const AVPacket& packet)
{
    double timeBase = av_q2d(stream->time_base);
    qint64 dts = m_videoStreamIndex != -1 ? m_formatContext->streams[m_videoStreamIndex]->first_dts : stream->first_dts;
    qint64 firstDts = (dts == AV_NOPTS_VALUE) ? 0 : dts;

    if (packet.dts != AV_NOPTS_VALUE)
    {
        double ttm = timeBase * (packet.dts - firstDts);
        return qint64(1e+6 * ttm);
    }
    else {
        return firstDts;
    }
}

QnCompressedAudioDataPtr QnAviArchiveDataProvider::getAudioData(const AVPacket& packet, AVStream* stream)
{
    int extra = 0;
    QnCompressedAudioDataPtr audioData = QnCompressedAudioDataPtr(new QnCompressedAudioData(CL_MEDIA_ALIGNMENT, packet.size + extra, stream->codec));
    CLByteArray& data = audioData->data;

    data.prepareToWrite(packet.size + extra);
    memcpy(data.data(), packet.data, packet.size);
    memset(data.data() + packet.size, 0, extra);
    data.done(packet.size + extra);

    audioData->compressionType = m_audioCodecId;
    //audioData->format.fromAvStream(stream->codec);

    double time_base = av_q2d(stream->time_base);
    audioData->timestamp = packetTimestamp(stream, packet);
    audioData->duration = qint64(1e+6 * time_base * packet.duration);

    return audioData;
}


QnCompressedVideoDataPtr QnAviArchiveDataProvider::getVideoData(const AVPacket& packet, AVCodecContext* codecContext)
{
    int extra = 0;
    QnCompressedVideoDataPtr videoData = QnCompressedVideoDataPtr(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, packet.size + extra, codecContext));
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

bool QnAviArchiveDataProvider::getNextVideoPacket()
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

void QnAviArchiveDataProvider::channeljumpTo(quint64 mksec, int /*channel*/)
{
    QMutexLocker mutex(&m_cs);

    mksec += startMksec();

    avformat_seek_file(m_formatContext, -1, 0, mksec, LLONG_MAX, AVSEEK_FLAG_BACKWARD);

    //m_needToSleep = 0;
    m_previousTime = -1;
    m_wakeup = true;
}

void QnAviArchiveDataProvider::setSkipFramesToTime(quint64 skip)
{
    QnNavigatedDataProvider::setSkipFramesToTime(skip);
    for (int channel = 0; channel < m_NumaberOfVideoChannels; ++channel)
    {
        m_skippedToTime[channel] = (skip == 0);
    }
}
