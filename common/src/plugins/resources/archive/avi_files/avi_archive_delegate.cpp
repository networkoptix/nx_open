#include "avi_archive_delegate.h"

#include <QtCore/QSemaphore>
#include <QtCore/QSharedPointer>

#include "stdint.h"
#include <libavformat/avformat.h>
#include "core/resource/resource_media_layout.h"
#include "utils/media/ffmpeg_helper.h"
#include "core/resource/storage_resource.h"

extern QMutex global_ffmpeg_mutex;


class QnAviAudioLayout: public QnResourceAudioLayout
{
public:
    QnAviAudioLayout(QnAviArchiveDelegate* owner):
        QnResourceAudioLayout(),
        m_owner(owner)
    {

    }

    virtual int numberOfChannels() const override
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

    virtual QnResourceAudioLayout::AudioTrack getAudioTrackInfo(int index) const override
    {
        QnResourceAudioLayout::AudioTrack result;
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
                //result.codec = codecContext->codec_id;
                result.codecContext =  QnMediaContextPtr(new QnMediaContext(codecContext));
                result.description = QString::number(++audioNumber);
                result.description += QLatin1String(". ");

                AVDictionaryEntry * lang = av_dict_get(strm->metadata, "language", 0, 0);
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
    m_selectedAudioChannel(0),
    m_startTime(0),
    m_useAbsolutePos(true),
    m_duration(AV_NOPTS_VALUE)
{
    close();
    m_audioLayout = new QnAviAudioLayout(this);
}

void QnAviArchiveDelegate::setUseAbsolutePos(bool value)
{
    m_useAbsolutePos = value;
}

QnAviArchiveDelegate::~QnAviArchiveDelegate()
{
    close();
    delete m_videoLayout;
    delete m_audioLayout;
}

qint64 QnAviArchiveDelegate::startTime()
{
    return m_startTime;
}

qint64 QnAviArchiveDelegate::endTime()
{
    //if (!m_streamsFound && !findStreams())
    //    return 0;
    if (m_duration == AV_NOPTS_VALUE)
        return m_duration;
    else
        return m_duration + m_startTime;
}

QnMediaContextPtr QnAviArchiveDelegate::getCodecContext(AVStream* stream)
{
    while (m_contexts.size() <= stream->index)
        m_contexts << QnMediaContextPtr(0);

    if (m_contexts[stream->index] == 0 || m_contexts[stream->index]->ctx()->codec_id != stream->codec->codec_id)
        m_contexts[stream->index] = QnMediaContextPtr(new QnMediaContext(stream->codec));

    return m_contexts[stream->index];
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

        if (m_indexToChannel.isEmpty())
            initLayoutStreams();

        switch(stream->codec->codec_type)
        {
            case AVMEDIA_TYPE_VIDEO:
                if (m_indexToChannel[packet.stream_index] == -1) {
                    av_free_packet(&packet);
                    continue;
                }
                videoData = new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, packet.size, getCodecContext(stream));
                videoData->channelNumber = m_indexToChannel[stream->index]; // [packet.stream_index]; // defalut value
                data = QnAbstractMediaDataPtr(videoData);
                break;
            case AVMEDIA_TYPE_AUDIO:
                if (packet.stream_index != m_audioStreamIndex || stream->codec->channels < 1 /*|| m_indexToChannel[packet.stream_index] == -1*/) {
                    av_free_packet(&packet);
                    continue;
                }

                audioData = new QnCompressedAudioData(CL_MEDIA_ALIGNMENT, packet.size, getCodecContext(stream));
                //audioData->format.fromAvStream(stream->codec);
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

qint64 QnAviArchiveDelegate::seek(qint64 time, bool findIFrame)
{
    time -= m_startTime;
    if (!findStreams())
        return -1;
    avformat_seek_file(m_formatContext, -1, 0, time + m_startMksec, LLONG_MAX, findIFrame ? AVSEEK_FLAG_BACKWARD : AVSEEK_FLAG_ANY);
    return time;
}

bool QnAviArchiveDelegate::open(QnResourcePtr resource)
{
    m_resource = resource;
    if (m_formatContext == 0)
    {
        QString url = m_resource->getUrl(); // "c:/00-1A-07-03-BD-09.mkv"; //
        if (m_storage == 0)
            m_storage = QnStorageResourcePtr(QnStoragePluginFactory::instance()->createStorage(url));


        m_formatContext = avformat_alloc_context();
        m_formatContext->pb = m_storage->createFfmpegIOContext(url, QIODevice::ReadOnly);
        if (!m_formatContext->pb) {
            close();
            return false;
        }
        m_initialized = avformat_open_input(&m_formatContext, "", 0, 0) >= 0;

        if (!m_initialized )
            close();

        getVideoLayout();
    }
    return m_initialized;
}

void QnAviArchiveDelegate::doNotFindStreamInfo()
{
    // this call used for optimization. Avoid av_find_stream_info_call for server's chunks to increase speed
    m_streamsFound=true;
    if (m_formatContext)
    {
        for (unsigned i = 0; i < m_formatContext->nb_streams; ++i)
            m_formatContext->streams[i]->first_dts = 0; // reset first_dts. If don't do it, av_seek will seek to begin of file always
    }
}

void QnAviArchiveDelegate::close()
{
    if (m_formatContext) {
        //avformat_close_input(&m_formatContext);
        m_storage->closeFfmpegIOContext(m_formatContext->pb);
        m_formatContext->pb = 0;
        avformat_free_context(m_formatContext);
    }
    m_contexts.clear();
    m_formatContext = 0;
    m_initialized = false;
    m_streamsFound = false;
    m_startMksec = 0;
    m_storage.clear();
}

static QnDefaultDeviceVideoLayout defaultVideoLayout;

QnVideoResourceLayout* QnAviArchiveDelegate::getVideoLayout()
{
    if (!m_initialized)
    {
        return &defaultVideoLayout;
    }

    if (m_videoLayout == 0)
    {
        m_videoLayout = new QnCustomDeviceVideoLayout(1, 1);

        // prevent standart tag name parsing in 'avi' format
        QString format = QString(m_formatContext->iformat->name).split(',')[0];

        AVDictionaryEntry* software = av_dict_get(m_formatContext->metadata, getTagName(Tag_Software, format), 0, 0);
        bool allowTags = format != QString("avi") || (software && QString(software->value) == QString("Network Optix"));

        if (allowTags)
        {
            AVDictionaryEntry* layoutInfo = av_dict_get(m_formatContext->metadata,getTagName(Tag_LayoutInfo, format), 0, 0);
            if (layoutInfo)
                deserializeLayout(m_videoLayout, layoutInfo->value);

            if (m_useAbsolutePos)
            {
                AVDictionaryEntry* start_time = av_dict_get(m_formatContext->metadata,getTagName(Tag_startTime, format), 0, 0);
                if (start_time)
                    m_startTime = QString(start_time->value).toLongLong()*1000ll;
            }
        }
    }
    return m_videoLayout;
}

QnResourceAudioLayout* QnAviArchiveDelegate::getAudioLayout()
{
    return m_audioLayout;
}

bool QnAviArchiveDelegate::findStreams()
{
    if (!m_initialized)
        return false;
    if (!m_streamsFound)
    {
        global_ffmpeg_mutex.lock();
        //m_streamsFound = m_formatContext->nb_streams > 0 || av_find_stream_info(m_formatContext) >= 0;
        m_streamsFound = av_find_stream_info(m_formatContext) >= 0;
        global_ffmpeg_mutex.unlock();
        if (m_streamsFound) 
        {
            m_duration = m_formatContext->duration;
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
            while (m_indexToChannel.size() <= static_cast<int>(i))
                m_indexToChannel << -1;
            m_indexToChannel[i] = videoNum;
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
    return qMax(0ll, (qint64) (timeBase * (packetTime - firstDts))) +  m_startTime;
}

bool QnAviArchiveDelegate::deserializeLayout(QnCustomDeviceVideoLayout* layout, const QString& layoutStr)
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
    if (!m_streamsFound && !findStreams())
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

bool QnAviArchiveDelegate::isStreamsFound() const
{
    return m_streamsFound;
}

void QnAviArchiveDelegate::setStorage(const QnStorageResourcePtr &storage)
{
    m_storage = storage;
}

const char* QnAviArchiveDelegate::getTagName(Tag tag, const QString& formatName)
{
    if (formatName == QString("avi"))
    {
        switch(tag)
        {
        case Tag_startTime:
            return "date"; // "ICRD";
        case Tag_endTime:
            return "ISRC"; // not used
        case Tag_LayoutInfo:
            return "comment"; // "ICMT";
        case Tag_Software:
            return "encoded_by"; // "ITCH";
        }
    }
    else {
        switch(tag)
        {
        case Tag_startTime:
            return "start_time"; // StartTimecode
        case Tag_endTime:
            return "end_time"; // EndTimecode
        case Tag_LayoutInfo:
            return "video_layout"; // TrackNumber
        case Tag_Software:
            return "software";
        }
    }
    return "";
}
