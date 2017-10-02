#include "avi_archive_delegate.h"

#include <QtCore/QSharedPointer>

#include "stdint.h"

extern "C"
{
#include <libavformat/avformat.h>
}

#include <utils/media/ffmpeg_helper.h>
#include <utils/media/av_codec_helper.h>
#include <nx/utils/log/log.h>
#include <utils/common/util.h>
#include <nx/fusion/model_functions.h>

#include <core/resource/storage_plugin_factory.h>

#include <core/resource/resource_media_layout.h>
#include <core/resource/storage_resource.h>
#include <core/resource/media_resource.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/av_codec_media_context.h>
#include <nx/streaming/config.h>
#include <core/ptz/media_dewarping_params.h>

#include <plugins/resource/avi/avi_resource.h>
#include <plugins/storage/file_storage/layout_storage_resource.h>

#include <motion/light_motion_archive_connection.h>
#include <export/sign_helper.h>
#include "utils/media/nalUnits.h"

class QnAviAudioLayout: public QnResourceAudioLayout
{
public:
    QnAviAudioLayout(QnAviArchiveDelegate* owner):
        QnResourceAudioLayout(),
        m_owner(owner)
    {

    }

    virtual int channelCount() const override
    {
        int audioNum = 0;
        int lastStreamID = -1;
        AVFormatContext* formatContext = m_owner->getFormatContext();
        if (!formatContext)
            return 0;
        for(unsigned i = 0; i < formatContext->nb_streams; i++)
        {
            AVStream *strm= formatContext->streams[i];
            if(strm->codec->codec_type >= AVMEDIA_TYPE_NB)
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

            if(codecContext->codec_type >= AVMEDIA_TYPE_NB)
                continue;

            if (strm->id && strm->id == lastStreamID)
                continue; // duplicate
            lastStreamID = strm->id;

            if (codecContext->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                if (audioNum++ < index)
                    continue;
                //result.codec = codecContext->codec_id;
                result.codecContext = QnConstMediaContextPtr(new QnAvCodecMediaContext(codecContext));
                result.description = QString::number(++audioNumber);
                result.description += QLatin1String(". ");

                AVDictionaryEntry * lang = av_dict_get(strm->metadata, "language", 0, 0);
                if (lang && lang->value && lang->value[0])
                {
                    QString langName = QString::fromLatin1(lang->value);
                    result.description += langName;
                    result.description += QLatin1String(" - ");
                }

                result.description = result.codecContext->getAudioCodecDescription();
                break;
            }
        }
        return result;
    }
private:
    QnAviArchiveDelegate* m_owner;
};


QnAviArchiveDelegate::QnAviArchiveDelegate():
    m_openMutex(QnMutex::Recursive)
{
    m_audioLayout.reset( new QnAviAudioLayout(this) );
    m_flags |= Flag_CanSendMotion;
}

void QnAviArchiveDelegate::setUseAbsolutePos(bool value)
{
    m_useAbsolutePos = value;
}

QnAviArchiveDelegate::~QnAviArchiveDelegate()
{
    close();
}

qint64 QnAviArchiveDelegate::startTime() const
{
    return m_startTimeUs;
}

void QnAviArchiveDelegate::setStartTimeUs(qint64 startTimeUs)
{
    m_startTimeUs = startTimeUs;
}

qint64 QnAviArchiveDelegate::endTime() const
{
    //if (!m_streamsFound && !findStreams())
    //    return 0;
    if (m_durationUs == qint64(AV_NOPTS_VALUE))
        return m_durationUs;
    else
        return m_durationUs + m_startTimeUs;
}

QnConstMediaContextPtr QnAviArchiveDelegate::getCodecContext(AVStream* stream)
{
    //if (stream->codec->codec_id == AV_CODEC_ID_MJPEG)
    //    return QnConstMediaContextPtr(nullptr);

    while (m_contexts.size() <= stream->index)
        m_contexts << QnConstMediaContextPtr(nullptr);

    if (m_contexts[stream->index] == 0 ||
        m_contexts[stream->index]->getCodecId() != stream->codec->codec_id)
    {
        m_contexts[stream->index] = QnConstMediaContextPtr(new QnAvCodecMediaContext(stream->codec));
    }

    return m_contexts[stream->index];
}

QnAbstractMediaDataPtr QnAviArchiveDelegate::getNextData()
{
    if (!findStreams() || m_eofReached)
        return QnAbstractMediaDataPtr();

    QnFfmpegAvPacket packet;
    QnAbstractMediaDataPtr data;
    AVStream *stream;
    while (1)
    {
        double time_base;

        if (av_read_frame(m_formatContext, &packet) < 0)
            return QnAbstractMediaDataPtr();
        stream= m_formatContext->streams[packet.stream_index];
        if (stream->codec->codec_id == AV_CODEC_ID_H264 && packet.size == 6)
        {
            // may be H264 delimiter as separate packet. remove it
            if (packet.data[0] == 0x00 && packet.data[1] == 0x00 && packet.data[2] == 0x00 && packet.data[3] == 0x01 && packet.data[4] == nuDelimiter)
                continue; // skip delimiter
        }

        if (m_indexToChannel.isEmpty())
            initLayoutStreams();

        switch(stream->codec->codec_type)
        {
            case AVMEDIA_TYPE_VIDEO:
            {
                if (m_indexToChannel[packet.stream_index] == -1) {
                    av_free_packet(&packet);
                    continue;
                }
                QnWritableCompressedVideoData* videoData = new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT, packet.size, getCodecContext(stream));
                videoData->channelNumber = m_indexToChannel[stream->index]; // [packet.stream_index]; // defalut value
                data = QnAbstractMediaDataPtr(videoData);
                packetTimestamp(videoData, packet);
                videoData->m_data.write((const char*) packet.data, packet.size);
                break;
            }

            case AVMEDIA_TYPE_AUDIO:
            {
                if (packet.stream_index != m_audioStreamIndex || stream->codec->channels < 1 /*|| m_indexToChannel[packet.stream_index] == -1*/) {
                    av_free_packet(&packet);
                    continue;
                }
                qint64 timestamp = packetTimestamp(packet);
                if (!hasVideo() && m_lastSeekTime != AV_NOPTS_VALUE && timestamp < m_lastSeekTime) {
                    av_free_packet(&packet);
                    continue; // seek is broken for audio only media streams
                }
                QnWritableCompressedAudioData* audioData = new QnWritableCompressedAudioData(CL_MEDIA_ALIGNMENT, packet.size, getCodecContext(stream));
                //audioData->format.fromAvStream(stream->codec);
                time_base = av_q2d(stream->time_base)*1e+6;
                audioData->duration = qint64(time_base * packet.duration);
                data = QnAbstractMediaDataPtr(audioData);
                audioData->channelNumber = m_indexToChannel[packet.stream_index];
                audioData->timestamp = timestamp;
                audioData->m_data.write((const char*) packet.data, packet.size);
                break;
            }

            default:
            {
                av_free_packet(&packet);
                continue;
            }
        }
        break;
    }

    data->compressionType = stream->codec->codec_id;
    data->flags = static_cast<QnAbstractMediaData::MediaFlags>(packet.flags);

    while (packet.stream_index >= m_lastPacketTimes.size())
        m_lastPacketTimes << m_startTimeUs;
    if (data->timestamp == AV_NOPTS_VALUE) {
        /*
        AVStream* stream = m_formatContext->streams[packet.stream_index];
        if (stream->r_frame_rate.num)
            m_lastPacketTimes[packet.stream_index] += 1000000ll * stream->r_frame_rate.den / stream->r_frame_rate.num;
        else if (stream->time_base.den)
            m_lastPacketTimes[packet.stream_index] += 1000000ll * stream->time_base.num / stream->time_base.den;
        */
        data->timestamp = m_lastPacketTimes[packet.stream_index];
    }
    else {
        m_lastPacketTimes[packet.stream_index] = data->timestamp;
    }

    av_free_packet(&packet);

    return data;
}

qint64 QnAviArchiveDelegate::seek(qint64 time, bool findIFrame)
{
    if (!findStreams())
        return -1;

    m_eofReached = time > endTime();
    if (m_eofReached)
        return time;

    qint64 relTime = qMax(time-m_startTimeUs, 0ll);
    if (m_hasVideo)
        av_seek_frame(m_formatContext, -1, relTime + m_playlistOffsetUs, findIFrame ? AVSEEK_FLAG_BACKWARD : AVSEEK_FLAG_ANY);
    else {
        // mp3 seek is bugged in current ffmpeg version
        if (!reopen())
            return -1;
    }
    m_lastSeekTime = relTime + m_playlistOffsetUs + m_startTimeUs; // file physical time to UTC time
    return time;
}

bool QnAviArchiveDelegate::reopen()
{
    auto storage = m_storage;
    close();
    m_storage = storage;
    if (!open(m_resource))
        return false;
    if (!findStreams())
        return false;

    return true;
}

bool QnAviArchiveDelegate::open(const QnResourcePtr &resource)
{
    QnMutexLocker lock( &m_openMutex ); // need refactor. Now open may be called from UI thread!!!

    m_resource = resource;
    if (m_formatContext == nullptr)
    {
        m_eofReached = false;
        QString url = m_resource->getUrl();
        if (m_storage == nullptr)
        {
            m_storage = QnStorageResourcePtr(
                QnStoragePluginFactory::instance()->createStorage(
                    resource->commonModule(),
                    url));
            if(!m_storage)
                return false;
        }

        m_formatContext = avformat_alloc_context();
        NX_ASSERT(m_formatContext != nullptr);
        if (m_formatContext == nullptr)
            return false;

        m_IOContext = QnFfmpegHelper::createFfmpegIOContext(m_storage, url, QIODevice::ReadOnly);
        if (!m_IOContext)
        {
            close();
            m_resource->setStatus(Qn::Offline); // mark local resource as unaccessible
            return false;
        }
        m_formatContext->pb = m_IOContext;
        /**
         * If avformat_open_input() fails, FormatCtx will be freed before avformat_open_input()
         * returns.
         */
        m_initialized = avformat_open_input(&m_formatContext, "", 0, 0) >= 0;
        if (!m_initialized)
        {
            close();
            m_resource->setStatus(Qn::Offline); // mark local resource as unaccessible
            return false;
        }

        initMetadata();
        getVideoLayout();
    }
    m_resource->setStatus(Qn::Online);
    return m_initialized;
}

/*
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
*/

void QnAviArchiveDelegate::close()
{
    if (m_IOContext)
    {
        QnFfmpegHelper::closeFfmpegIOContext(m_IOContext);
        if (m_formatContext)
            m_formatContext->pb = nullptr;
        m_IOContext = nullptr;
    }

	if (m_formatContext)
		avformat_close_input(&m_formatContext);

    m_contexts.clear();
    m_formatContext = nullptr;
    m_initialized = false;
    m_streamsFound = false;
    m_playlistOffsetUs = 0;
    m_storage.clear();
    m_lastPacketTimes.clear();
    m_lastSeekTime = AV_NOPTS_VALUE;
}

const char* QnAviArchiveDelegate::getTagValue( const char* tagName )
{
    if (!m_initialized)
        return 0;
    AVDictionaryEntry* entry = av_dict_get(m_formatContext->metadata, tagName, 0, 0);
    return entry ? entry->value : 0;
}

QnAviArchiveMetadata QnAviArchiveDelegate::metadata() const
{
    return m_metadata;
}

bool QnAviArchiveDelegate::hasVideo() const
{
    auto nonConstThis = const_cast<QnAviArchiveDelegate*> (this);
    nonConstThis->findStreams();
    return m_hasVideo;
}

static QSharedPointer<QnDefaultResourceVideoLayout> defaultVideoLayout( new QnDefaultResourceVideoLayout() );

QnConstResourceVideoLayoutPtr QnAviArchiveDelegate::getVideoLayout()
{
    if (!m_initialized)
        return defaultVideoLayout;

    // TODO: #rvasilenko why do we init video layout in the semantically const method?
    if (!m_videoLayout)
    {
        m_videoLayout.reset( new QnCustomResourceVideoLayout(QSize(1, 1)) );

        if (!m_metadata.videoLayoutSize.isEmpty())
        {
            m_videoLayout->setSize(m_metadata.videoLayoutSize);
            m_videoLayout->setChannels(m_metadata.videoLayoutChannels);
        }

        if (m_useAbsolutePos)
        {
            m_startTimeUs = 1000ll * m_metadata.startTimeMs;
            if (m_startTimeUs >= UTC_TIME_DETECTION_THRESHOLD)
            {
                m_resource->addFlags(Qn::utc | Qn::exported);
                if (qSharedPointerDynamicCast<QnLayoutFileStorageResource>(m_storage))
                    m_resource->addFlags(Qn::sync | Qn::periods | Qn::motion); // use sync for exported layout only
            }
        }
    }


    return m_videoLayout;
}

QnConstResourceAudioLayoutPtr QnAviArchiveDelegate::getAudioLayout()
{
    return m_audioLayout;
}

extern "C" {
int interruptDetailFindStreamInfo(void*)
{
    return 1;
}
};

bool QnAviArchiveDelegate::findStreams()
{
    if (!m_initialized)
        return false;
    if (!m_streamsFound)
    {
        if (m_fastStreamFind) {
            m_formatContext->interrupt_callback.callback = &interruptDetailFindStreamInfo;
            avformat_find_stream_info(m_formatContext, nullptr);
            m_formatContext->interrupt_callback.callback = 0;
            m_streamsFound = m_formatContext->nb_streams > 0;
            for (unsigned i = 0; i < m_formatContext->nb_streams; ++i)
                m_formatContext->streams[i]->first_dts = 0; // reset first_dts. If don't do it, av_seek will seek to begin of file always
        }
        else {
            // TODO: #vasilenko avoid using deprecated methods
            m_streamsFound = avformat_find_stream_info(m_formatContext, nullptr) >= 0;
        }

        if (m_streamsFound)
        {
            m_durationUs = m_formatContext->duration;
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
    int lastStreamID = -1;
    m_firstVideoIndex = -1;
    m_hasVideo = false;
    for(unsigned i = 0; i < m_formatContext->nb_streams; i++)
    {
        AVStream *strm= m_formatContext->streams[i];
        AVCodecContext *codecContext = strm->codec;

        if(codecContext->codec_type >= AVMEDIA_TYPE_NB)
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
            m_hasVideo = true;
            break;

        default:
            break;
        }
    }

    lastStreamID = -1;
    int audioNum = 0;
    for(unsigned i = 0; i < m_formatContext->nb_streams; i++)
    {
        AVStream *strm= m_formatContext->streams[i];
        AVCodecContext *codecContext = strm->codec;

        if(codecContext->codec_type >= AVMEDIA_TYPE_NB)
            continue;

        if (strm->id && strm->id == lastStreamID)
            continue; // duplicate
        lastStreamID = strm->id;

        switch(codecContext->codec_type)
        {
        case AVMEDIA_TYPE_AUDIO:
            while ((uint)m_indexToChannel.size() <= i)
                m_indexToChannel << -1;
            m_indexToChannel[i] = videoNum + audioNum++;
            break;

        default:
            break;
        }
    }
}

void QnAviArchiveDelegate::initMetadata()
{
    auto aviRes = m_resource.dynamicCast<QnAviResource>();
    if (aviRes && aviRes->hasAviMetadata())
    {
        m_metadata = aviRes->aviMetadata();
        return;
    }

    m_metadata = QnAviArchiveMetadata::loadFromFile(m_formatContext);

    if (aviRes)
    {
        aviRes->setAviMetadata(m_metadata);
        if (m_metadata.timeZoneOffset != Qn::InvalidUtcOffset)
            aviRes->setTimeZoneOffset(m_metadata.timeZoneOffset);
    }
}

qint64 QnAviArchiveDelegate::packetTimestamp(const AVPacket& packet)
{
    AVStream* stream = m_formatContext->streams[packet.stream_index];
    double timeBase = av_q2d(stream->time_base) * 1000000;
    qint64 firstDts = m_firstVideoIndex >= 0 ? m_formatContext->streams[m_firstVideoIndex]->first_dts : 0;
    if (firstDts == qint64(AV_NOPTS_VALUE))
        firstDts = 0;
    qint64 packetTime = packet.dts != qint64(AV_NOPTS_VALUE) ? packet.dts : packet.pts;
    //qint64 packetTime = qMax(packet.dts, packet.pts);
    if (packetTime == qint64(AV_NOPTS_VALUE))
        return AV_NOPTS_VALUE;
    else
        return qMax(0ll, (qint64) (timeBase * (packetTime - firstDts))) +  m_startTimeUs;
}

void QnAviArchiveDelegate::packetTimestamp(QnCompressedVideoData* video, const AVPacket& packet)
{
    AVStream* stream = m_formatContext->streams[packet.stream_index];
    double timeBase = av_q2d(stream->time_base) * 1000000;
    qint64 firstDts = m_firstVideoIndex >= 0 ? m_formatContext->streams[m_firstVideoIndex]->first_dts : 0;
    if (firstDts == qint64(AV_NOPTS_VALUE))
        firstDts = 0;
    qint64 packetTime = packet.dts != qint64(AV_NOPTS_VALUE) ? packet.dts : packet.pts;
    if (packetTime == qint64(AV_NOPTS_VALUE))
        video->timestamp = AV_NOPTS_VALUE;
    else
        video->timestamp = qMax(0ll, (qint64) (timeBase * (packetTime - firstDts))) +  m_startTimeUs;
    if (packet.pts != AV_NOPTS_VALUE) {
        video->pts = qMax(0ll, (qint64) (timeBase * (packet.pts - firstDts))) +  m_startTimeUs;
    }
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
    int currentAudioTrackNum = 0;
    for (unsigned i = 0; i < m_formatContext->nb_streams; i++)
    {
        AVStream *strm= m_formatContext->streams[i];
        AVCodecContext *codecContext = strm->codec;

        if(codecContext->codec_type >= AVMEDIA_TYPE_NB)
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

void QnAviArchiveDelegate::setFastStreamFind(bool value)
{
    m_fastStreamFind = value;
}

QnAbstractMotionArchiveConnectionPtr QnAviArchiveDelegate::getMotionConnection(int channel)
{
    QnAviResourcePtr aviResource = m_resource.dynamicCast<QnAviResource>();
    if (!aviResource)
        return QnAbstractMotionArchiveConnectionPtr();
    const QnMetaDataLightVector& motionData = aviResource->getMotionBuffer(channel);
    return QnAbstractMotionArchiveConnectionPtr(new QnLightMotionArchiveConnection(motionData, channel));
}

void QnAviArchiveDelegate::setMotionRegion(const QnMotionRegion& /*region*/)
{
    // Do nothing.
}

/*
void QnAviArchiveDelegate::setMotionConnection(QnAbstractMotionArchiveConnectionPtr connection, int channel)
{
    while (m_motionConnections.size() <= channel)
        m_motionConnections << QnAbstractMotionArchiveConnectionPtr();
    m_motionConnections[channel] = connection;
}
*/
