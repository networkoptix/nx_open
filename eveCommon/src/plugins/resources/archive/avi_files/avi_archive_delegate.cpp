#include "avi_archive_delegate.h"
#include "data/mediadata.h"
#include "stdint.h"
#include <libavformat/avformat.h>
#include "base/ffmpeg_helper.h"
#include "decoders/audio/ffmpeg_audio.h"
#include "decoders/video/frame_info.h"
#include "device/device_video_layout.h"

extern QMutex global_ffmpeg_mutex;

class QnAviAudioLayout: public QnDeviceAudioLayout
{
public:
    QnAviAudioLayout(QnAviArchiveDelegate* owner):
        QnDeviceAudioLayout(),
        m_owner(owner)
    {

    }

    unsigned int numberOfChannels() const 
    {
        int audioNum = 0;
        int lastStreamID = -1;
        AVFormatContext* formatContext = m_owner->getFormatContext();
        if (!formatContext)
            return 0;
        for(unsigned i = 0; i < formatContext->nb_streams; i++)
        {
            AVStream *strm= formatContext->streams[i];
            if(strm->codec->codec_type >= (unsigned)AVMEDIA_TYPE_NB)
                continue;
            if (strm->id && strm->id == lastStreamID)
                continue; // duplicate
            lastStreamID = strm->id;
            if (strm->codec->codec_type == AVMEDIA_TYPE_AUDIO)
                audioNum++;
        }
        return audioNum;
    }

    virtual QnDeviceAudioLayout::AudioTrack getAudioTrackInfo(int index) 
    {
        QnDeviceAudioLayout::AudioTrack result;
        AVFormatContext* formatContext = m_owner->getFormatContext();
        if (formatContext == 0)
            return result;
        int audioNumber = 0;
        int lastStreamID = -1;
        int audioNum = 0;
        for(unsigned i = 0; i < formatContext->nb_streams; i++)
        {
            AVStream *strm= formatContext->streams[i];

            AVCodecContext *codecContext = strm->codec;

            if(codecContext->codec_type >= (unsigned)AVMEDIA_TYPE_NB)
                continue;

            if (strm->id && strm->id == lastStreamID)
                continue; // duplicate
            lastStreamID = strm->id;

            if (codecContext->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                if (audioNum++ < index)
                    continue;
                result.codec = codecContext->codec_id;
                result.description = QString::number(++audioNumber);
                result.description += QLatin1String(". ");

                AVMetadataTag* lang = av_metadata_get(strm->metadata, "language", 0, 0);
                if (lang && lang->value && lang->value[0])
                {
                    QString langName = QString::fromLatin1(lang->value);
                    result.description += langName;
                    result.description += QLatin1String(" - ");
                }

                QString codecStr = codecIDToString(codecContext->codec_id);
                if (!codecStr.isEmpty())
                {
                    result.description += codecStr;
                    result.description += QLatin1Char(' ');
                }

                //str += QString::number(codecContext->sample_rate / 1000)+ QLatin1String("Khz ");
                if (codecContext->channels == 3)
                    result.description += QLatin1String("2.1");
                else if (codecContext->channels == 6)
                    result.description += QLatin1String("5.1");
                else if (codecContext->channels == 8)
                    result.description += QLatin1String("7.1");
                else if (codecContext->channels == 2)
                    result.description += QLatin1String("stereo");
                else if (codecContext->channels == 1)
                    result.description += QLatin1String("mono");
                else
                    result.description += QString::number(codecContext->channels);
            }
        }
        return result;
    }
private:
    QnAviArchiveDelegate* m_owner;
};

QnAviArchiveDelegate::QnAviArchiveDelegate():
    m_formatContext(0),
    m_videoLayout(0),
    m_firstVideoIndex(0),
    m_audioStreamIndex(-1),
    m_selectedAudioChannel(0)
{
    close();
    m_audioLayout = new QnAviAudioLayout(this);
}

QnAviArchiveDelegate::~QnAviArchiveDelegate()
{
    close();
    delete m_videoLayout;
    delete m_audioLayout;
}

qint64 QnAviArchiveDelegate::startTime()
{
    return 0;
}

qint64 QnAviArchiveDelegate::endTime()
{
    if (!m_streamsFound && !findStreams()) 
        return 0;
    return contentLength();
}

QnAbstractMediaDataPtr QnAviArchiveDelegate::getNextData()
{
    if (!findStreams())
        return QnAbstractMediaDataPtr();

    AVPacket packet;
    QnAbstractMediaDataPtr data;
    QnCompressedVideoData* videoData;
    QnCompressedAudioData* audioData;
    AVStream *stream;
    while (1)
    {
        double time_base;

        if (av_read_frame(m_formatContext, &packet) < 0)
            return QnAbstractMediaDataPtr();
        
        stream= m_formatContext->streams[packet.stream_index];
        switch(stream->codec->codec_type) 
        {
            case AVMEDIA_TYPE_VIDEO:
                if (m_indexToChannel[packet.stream_index] == -1) {
                    av_free_packet(&packet);
                    continue;
                }
                videoData = new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, packet.size, stream->codec);
                videoData->channelNumber = m_indexToChannel[stream->index]; // [packet.stream_index]; // defalut value
                data = QnAbstractMediaDataPtr(videoData);
                break;
            case AVMEDIA_TYPE_AUDIO:
                if (packet.stream_index != m_audioStreamIndex || stream->codec->channels < 1 /*|| m_indexToChannel[packet.stream_index] == -1*/) {
                    av_free_packet(&packet);
                    continue;
                }

                audioData = new QnCompressedAudioData(CL_MEDIA_ALIGNMENT, packet.size, stream->codec);
                audioData->format.fromAvStream(stream->codec);
                time_base = av_q2d(stream->time_base)*1e+6;
                audioData->duration = qint64(time_base * packet.duration);
                data = QnAbstractMediaDataPtr(audioData);
                audioData->channelNumber = stream->index; // m_indexToChannel[packet.stream_index]; // defalut value
                break;
            default:
                av_free_packet(&packet);
                continue;
        }
        break;
    }

    data->data.write((const char*) packet.data, packet.size);
    data->compressionType = stream->codec->codec_id;
    data->flags = packet.flags;
    data->timestamp = packetTimestamp(packet);

    av_free_packet(&packet);
    
    return data;
}

qint64 QnAviArchiveDelegate::seek(qint64 time)
{
    if (!findStreams())
        return -1;
    avformat_seek_file(m_formatContext, -1, 0, time + m_startMksec, LLONG_MAX, AVSEEK_FLAG_BACKWARD);
    return time;
}

bool QnAviArchiveDelegate::open(const QnResource* resource)
{
    m_resource = resource;
    if (m_formatContext == 0) 
    {
        QString url = QLatin1String("ufile:") + m_resource->getUrl();
        m_initialized = av_open_input_file(&m_formatContext, url.toUtf8().constData(), NULL, 0, NULL) >= 0;
        if (!m_initialized ) 
            close();
    }
    return m_initialized;
}

void QnAviArchiveDelegate::close()
{
    if (m_formatContext)
        av_close_input_file(m_formatContext);
    m_formatContext = 0;
    m_initialized = false;
    m_streamsFound = false;
    m_startMksec = 0;
}

QnDeviceVideoLayout* QnAviArchiveDelegate::getVideoLayout()
{
    if (!m_initialized) {
        return 0;
    }
    if (m_videoLayout == 0)
    {
        m_videoLayout = new CLCustomDeviceVideoLayout(1, 1);
        AVMetadataTag* layoutInfo = av_metadata_get(m_formatContext->metadata,"video_layout", 0, 0);
        if (layoutInfo)
            deserializeLayout(m_videoLayout, layoutInfo->value);
    }
    return m_videoLayout;
}

QnDeviceAudioLayout* QnAviArchiveDelegate::getAudioLayout()
{
    return m_audioLayout;
}

bool QnAviArchiveDelegate::findStreams()
{
    if (!m_initialized)
        return 0;
    if (!m_streamsFound) 
    {
        QMutexLocker global_ffmpeg_locker(&global_ffmpeg_mutex);
        m_streamsFound = av_find_stream_info(m_formatContext) >= 0;
        if (m_streamsFound) {
            if (m_formatContext->start_time != AV_NOPTS_VALUE)
                m_startMksec = m_formatContext->start_time;
            if ((m_startMksec == 0 || m_startMksec == AV_NOPTS_VALUE) && m_formatContext->streams > 0 && m_formatContext->streams[0]->first_dts != AV_NOPTS_VALUE)
            {
                AVStream* stream = m_formatContext->streams[0];
                double timeBase = av_q2d(stream->time_base) * 1000000;
                m_startMksec = stream->first_dts * timeBase;
            }
            initLayoutStreams();
        }
        else {
            close();
        }
    }
    return m_streamsFound;
}

void QnAviArchiveDelegate::initLayoutStreams()
{
    int videoNum= 0;
    //int audioNum = 0;
    int lastStreamID = -1;
    m_firstVideoIndex = -1;

    AVDictionaryEntry* entry;
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
            if (m_firstVideoIndex == -1)
                m_firstVideoIndex = i;
            entry = av_metadata_get(m_formatContext->streams[i]->metadata, "layout_channel", 0, 0);
            while (m_indexToChannel.size() <= i)
                m_indexToChannel << -1;
            if (entry) {
                int channel = QString(entry->value).toInt();
                m_indexToChannel[i] = channel;
            }
            else if (videoNum == 0) {
                m_indexToChannel[i] = 0;
            }
            videoNum++;
            break;

        case AVMEDIA_TYPE_AUDIO:
            //while (m_indexToChannel.size() <= i)
            //    m_indexToChannel << -1;
            //m_indexToChannel << audioNum++;
            break;

        default:
            break;
        }
    }
}

qint64 QnAviArchiveDelegate::contentLength() const 
{ 
    return m_formatContext->duration; 
}

qint64 QnAviArchiveDelegate::packetTimestamp(const AVPacket& packet)
{
    AVStream* stream = m_formatContext->streams[packet.stream_index];
    double timeBase = av_q2d(stream->time_base) * 1000000;
    qint64 firstDts = m_firstVideoIndex >= 0 ? m_formatContext->streams[m_firstVideoIndex]->first_dts : 0;
    if (firstDts == AV_NOPTS_VALUE)
        firstDts = 0;
    qint64 packetTime = packet.dts != AV_NOPTS_VALUE ? packet.dts : packet.pts;
    if (packetTime == AV_NOPTS_VALUE)
        return AV_NOPTS_VALUE;
    return qMax(0ll, (qint64) (timeBase * (packetTime - firstDts)));
}

bool QnAviArchiveDelegate::deserializeLayout(CLCustomDeviceVideoLayout* layout, const QString& layoutStr)
{
    QStringList info = layoutStr.split(';');
    for (int i = 0; i < info.size(); ++i)
    {
        QStringList params = info[i].split(',');
        if (params.size() != 2) {
            cl_log.log("Invalid layout string stored at file metadata. Ignored.", cl_logWARNING);
            return false;
        }
        if (i == 0) {
            layout->setWidth(params[0].toInt());
            layout->setHeight(params[1].toInt());
        }
        else {
            layout->setChannel(params[0].toInt(), params[1].toInt(), i-1);
        }
    }
    return true;
}

AVCodecContext* QnAviArchiveDelegate::setAudioChannel(int num)
{
    if (!m_formatContext)
        return 0;
    // convert num to absolute track number
    m_audioStreamIndex = -1;
    int lastStreamID = -1;
    unsigned currentAudioTrackNum = 0;
    for (unsigned i = 0; i < m_formatContext->nb_streams; i++)
    {
        AVStream *strm= m_formatContext->streams[i];
        AVCodecContext *codecContext = strm->codec;

        if(codecContext->codec_type >= (unsigned)AVMEDIA_TYPE_NB)
            continue;

        if (strm->id && strm->id == lastStreamID)
            continue; // duplicate
        lastStreamID = strm->id;

        if (codecContext->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            if (currentAudioTrackNum == num)
            {
                m_audioStreamIndex = i;
                m_selectedAudioChannel = num;
                return m_formatContext->streams[m_audioStreamIndex]->codec;
            }
            currentAudioTrackNum++;
        }
    }
    return 0;
}

AVFormatContext* QnAviArchiveDelegate::getFormatContext() 
{ 
    if (!m_streamsFound && !findStreams()) 
        return 0;
    return m_formatContext; 
}
