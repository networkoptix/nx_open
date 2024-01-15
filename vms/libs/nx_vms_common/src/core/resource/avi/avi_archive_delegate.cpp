// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "avi_archive_delegate.h"

#include <chrono>

#include <stdint.h>

#include <QtCore/QDataStream>
#include <QtCore/QSharedPointer>

#include <analytics/common/object_metadata.h>
#include <analytics/common/object_metadata_v0.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/resource_media_layout.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource/storage_resource.h>
#include <core/storage/file_storage/layout_storage_resource.h>
#include <export/sign_helper.h>
#include <motion/light_motion_archive_connection.h>
#include <nx/codec/h263/h263_utils.h>
#include <nx/codec/nal_units.h>
#include <nx/fusion/model_functions.h>
#include <nx/media/av_codec_helper.h>
#include <nx/media/codec_parameters.h>
#include <nx/media/config.h>
#include <nx/media/ffmpeg_helper.h>
#include <nx/media/video_data_packet.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data/storage_init_result.h>
#include <nx/vms/common/application_context.h>
#include <recording/helpers/recording_context_helpers.h>
#include <utils/common/util.h>
#include <utils/media/ffmpeg_io_context.h>

extern "C" {
#include <libavformat/avformat.h>
} // extern "C"

using namespace nx::vms::common;

namespace {

// Try to reopen file if seek was faulty and seek time doesn't exceeds this constant.
static const std::chrono::microseconds kMaxFaultySeekReopenOffset = std::chrono::seconds(15);
static const qint64 kSeekError = -1;

/**
 * Parses h264 NAL unit list to determine if it contains I frame (IDR or non-IDR).
 * Since we may have multiple NAL units without actual data (such as SPS, PPS), we have to iterate
 * them until we meet the one with data to check if it's a I frame. That might be time-consuming,
 * so this function should not be used after at least one key frame has been found.
 */
static bool isH264IFrame(const QnAbstractMediaDataPtr& data)
{
    if (data->context->getCodecId() != AV_CODEC_ID_H264)
        return false;

    const quint8* pData = (const quint8*)data->data();
    const quint8* pDataEnd = pData + data->dataSize();
    while ((pData = NALUnit::findNextNAL(pData, pDataEnd)) != pDataEnd)
    {
        if (NALUnit::isIFrame(pData, pDataEnd - pData))
            return true;
    }

    return false;
}

static void updateKeyFrameFlagH263(QnAbstractMediaDataPtr& data)
{
    nx::media::h263::PictureHeader header;
    if (header.decode((uint8_t*)data->data(), data->dataSize()))
    {
        data->flags.setFlag(
            QnAbstractMediaData::MediaFlag::MediaFlags_AVKey,
            nx::media::h263::isKeyFrame(header.pictureType));
    }
}

} // namespace

QnAviArchiveDelegate::QnAviArchiveDelegate():
    m_openMutex(nx::Mutex::Recursive)
{
    m_flags |= Flag_CanSendMetadata;
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
    if (m_durationUs == qint64(AV_NOPTS_VALUE))
        return m_durationUs;
    else
        return m_durationUs + m_startTimeUs;
}

CodecParametersConstPtr QnAviArchiveDelegate::getCodecContext(AVStream* stream)
{
    while (m_contexts.size() <= stream->index)
        m_contexts << CodecParametersConstPtr(nullptr);

    if (m_contexts[stream->index] == 0 ||
        m_contexts[stream->index]->getCodecId() != stream->codecpar->codec_id)
    {
        m_contexts[stream->index] = std::make_shared<CodecParameters>(stream->codecpar);
    }

    return m_contexts[stream->index];
}

void convertMetadataFromOldFormat(QnCompressedMetadata* packet, int version)
{
    if (version != 0)
        return; //< nothing to convert.

    auto buffer = QByteArray::fromRawData(packet->data(), int(packet->dataSize()));
    bool success = false;
    using namespace nx::common::metadata;
    auto oldPacket = QnUbjson::deserialized<ObjectMetadataPacketV0>(buffer, ObjectMetadataPacketV0(), &success);
    if (!success)
        return; //< Don't try to convert;

    ObjectMetadataPacket newPacket;
    newPacket.deviceId = oldPacket.deviceId;
    newPacket.timestampUs = oldPacket.timestampUs;
    newPacket.durationUs = oldPacket.durationUs;
    for (const auto& oldMetadata: oldPacket.objectMetadataList)
    {
        ObjectMetadata newMetadata;
        newMetadata.typeId = oldMetadata.typeId;
        newMetadata.trackId = oldMetadata.trackId;
        newMetadata.boundingBox = oldMetadata.boundingBox;
        newMetadata.attributes = oldMetadata.attributes;
        newMetadata.objectMetadataType = oldMetadata.bestShot
            ? ObjectMetadataType::bestShot
            : ObjectMetadataType::regular;

        newMetadata.analyticsEngineId = oldMetadata.analyticsEngineId;
        newPacket.objectMetadataList.push_back(newMetadata);
    }
    newPacket.streamIndex = oldPacket.streamIndex;

    packet->setData(QnUbjson::serialized(newPacket));
}

bool QnAviArchiveDelegate::readFailed() const
{
    return m_readFailed;
}

QnAbstractMediaDataPtr QnAviArchiveDelegate::getNextData()
{
    if (!findStreams() || m_eofReached)
        return QnAbstractMediaDataPtr();

    QnAbstractMediaDataPtr data;
    AVStream *stream;
    while (1)
    {
        QnFfmpegAvPacket packet;
        double time_base;
        int status = av_read_frame(m_formatContext, &packet);
        if (status < 0)
        {
            if (status != AVERROR_EOF)
            {
                NX_DEBUG(this, "Failed to read frame: %1", QnFfmpegHelper::avErrorToString(status));
                m_readFailed = true;
            }
            return QnAbstractMediaDataPtr();
        }

        stream = m_formatContext->streams[packet.stream_index];
        if (stream->codecpar->codec_id == AV_CODEC_ID_H264 && packet.size == 6)
        {
            // may be H264 delimiter as separate packet. remove it
            if (packet.data[0] == 0x00 && packet.data[1] == 0x00 && packet.data[2] == 0x00 && packet.data[3] == 0x01 && packet.data[4] == nuDelimiter)
                continue; // skip delimiter
        }

        if (m_indexToChannel.empty())
            initLayoutStreams();

        auto indexToChannelIt = m_indexToChannel.find(packet.stream_index);
        if (indexToChannelIt == m_indexToChannel.cend())
        {
            NX_DEBUG(this, "Skip packet, stream number: %1, overall streams: %2",
                packet.stream_index, m_indexToChannel.size());
            continue;
        }

        switch(stream->codecpar->codec_type)
        {
            case AVMEDIA_TYPE_VIDEO:
            {
                auto videoData = new QnWritableCompressedVideoData(packet.size,
                    getCodecContext(stream));
                videoData->channelNumber = indexToChannelIt->second;
                if (stream->codecpar->width > 16 && stream->codecpar->height > 16)
                {
                    videoData->width = stream->codecpar->width;
                    videoData->height = stream->codecpar->height;
                }
                data = QnAbstractMediaDataPtr(videoData);
                packetTimestamp(videoData, packet);
                videoData->m_data.write((const char*) packet.data, packet.size);
                break;
            }

            case AVMEDIA_TYPE_AUDIO:
            {
                if (packet.stream_index != m_audioStreamIndex || stream->codecpar->channels < 1)
                    continue;

                qint64 timestamp = packetTimestamp(packet);
                if (!hasVideo() && m_lastSeekTime != AV_NOPTS_VALUE && timestamp < m_lastSeekTime)
                    continue; // seek is broken for audio only media streams

                QnWritableCompressedAudioData* audioData = new QnWritableCompressedAudioData(
                    packet.size, getCodecContext(stream));
                //audioData->format.fromAvStream(stream->codec);
                time_base = av_q2d(stream->time_base)*1e+6;
                audioData->duration = qint64(time_base * packet.duration);
                data = QnAbstractMediaDataPtr(audioData);
                audioData->channelNumber = indexToChannelIt->second;
                audioData->timestamp = timestamp;
                audioData->m_data.write((const char*) packet.data, packet.size);
                break;
            }
            case AVMEDIA_TYPE_SUBTITLE:
            {
                QnAbstractCompressedMetadataPtr metadata;
                if (m_metadata.metadataStreamVersion
                    >= QnAviArchiveMetadata::kMetadataStreamVersion_2)
                {
                    const auto packetData = QByteArray((const char*)packet.data, packet.size);
                    metadata = nx::recording::helpers::deserializeMetaDataPacket(packetData);
                }
                else
                {
                    auto objectDetection = std::make_shared<QnCompressedMetadata>(
                        MetadataType::ObjectDetection, packet.size);

                    objectDetection->m_data.write((const char*)packet.data, packet.size);
                    convertMetadataFromOldFormat(
                        objectDetection.get(), m_metadata.metadataStreamVersion);

                    metadata = objectDetection;
                }

                if (!metadata)
                {
                    NX_DEBUG(this, "Failed to read a metadata packet");
                    continue;
                }

                metadata->timestamp = packetTimestamp(packet);
                data = QnAbstractMediaDataPtr(metadata);
                break;
            }
            default:
            {
                continue;
            }
        }

        if (packet.flags & AV_PKT_FLAG_KEY)
            data->flags |= QnAbstractMediaData::MediaFlags_AVKey;

        while (packet.stream_index >= m_lastPacketTimes.size())
            m_lastPacketTimes << m_startTimeUs;
        if (data->timestamp == AV_NOPTS_VALUE)
            data->timestamp = m_lastPacketTimes[packet.stream_index];
        else
            m_lastPacketTimes[packet.stream_index] = data->timestamp;

        break;
    }

    if (data->dataType != QnAbstractMediaData::GENERIC_METADATA)
        data->compressionType = stream->codecpar->codec_id;

    // ffmpeg sets key frame flag for every received h263 frames (bug?), update flag manually.
    if (data->compressionType == AV_CODEC_ID_H263)
        updateKeyFrameFlagH263(data);

    /**
     * When parsing h264 stream FFMPEG fills MediaFlags_AVKey for IDR frames but won't do it for
     * non-IDR ones. That's why we check for such frames manually, but only one time per file open
     * or per seek.
     */
    if (m_keyFrameFound.size() > data->channelNumber && !m_keyFrameFound[data->channelNumber]
        && (data->flags.testFlag(QnAbstractMediaData::MediaFlag::MediaFlags_AVKey)
            || isH264IFrame(data)))
    {
        data->flags |= QnAbstractMediaData::MediaFlag::MediaFlags_AVKey;
        m_keyFrameFound[data->channelNumber] = true;
    }
    data->encryptionData = m_metadata.encryptionData;
    if (m_metadata.metadataStreamVersion <= QnAviArchiveMetadata::kMetadataStreamVersion_2)
        data->flags |= QnAbstractMediaData::MediaFlags_BomDecoding;
    return data;
}

bool QnAviArchiveDelegate::providesMotionPackets() const
{
    return m_metadata.metadataStreamVersion >= QnAviArchiveMetadata::kMetadataStreamVersion_2;
}

qint64 QnAviArchiveDelegate::seekWithFallback(qint64 time, bool findIFrame)
{
    // TODO: #lbusygin check is it still needed, and remove it(just check on single GOP file)
    auto resultTime = seek(time, findIFrame);
    if (resultTime == kSeekError)
    {
        // Try to reopen stream
        if (std::chrono::microseconds(time) > kMaxFaultySeekReopenOffset)
            return kSeekError;

        if (!reopen())
        {
            NX_DEBUG(this, "Cannot reopen file after faulty seek. Resource URL: %1",
                nx::utils::url::hidePassword(m_resource->getUrl()));

            return kSeekError;
        }
        return time;
    };

    return resultTime;
}

qint64 QnAviArchiveDelegate::seek(qint64 time, bool findIFrame)
{
    if (!findStreams())
        return kSeekError;

    const auto endTime = this->endTime();
    if (endTime != AV_NOPTS_VALUE)
    {
        m_eofReached = time > endTime;
        if (m_eofReached)
            return DATETIME_NOW;
    }

    std::fill(m_keyFrameFound.begin(), m_keyFrameFound.end(), false);
    const auto timeToSeek = qMax(time - m_startTimeUs, 0ll);
    if (m_hasVideo)
    {
        const auto result = av_seek_frame(
            m_formatContext, /*stream_index*/ -1, timeToSeek,
            findIFrame ? AVSEEK_FLAG_BACKWARD : AVSEEK_FLAG_ANY);

        if (result < 0)
        {
            NX_DEBUG(this,
                "Cannot seek into position %1. Resource URL: %2, av_seek_frame result: %3.",
                timeToSeek, nx::utils::url::hidePassword(m_resource->getUrl()), result);
            return kSeekError;
        }
    }
    else
    {
        // MP3 seek is buggy in the current ffmpeg version.
        if (!reopen())
            return kSeekError;
    }

    m_lastSeekTime = timeToSeek + m_startTimeUs; //< File physical time to UTC time.
    return time;
}

bool QnAviArchiveDelegate::reopen()
{
    auto storage = m_storage;
    close();
    m_storage = storage;
    if (!open(m_resource, m_archiveIntegrityWatcher))
        return false;

    return true;
}

bool QnAviArchiveDelegate::checkStorage()
{
    if (m_storage)
        return true;

    m_storage = QnStorageResourcePtr(
        appContext()->storagePluginFactory()->createStorage(m_resource->getUrl()));

    auto aviResource = m_resource.dynamicCast<QnAviResource>();
    if (aviResource)
    {
        auto aviStorage = aviResource->getStorage().dynamicCast<QnLayoutFileStorageResource>();
        auto fileStorage = m_storage.dynamicCast<QnLayoutFileStorageResource>();
        if (fileStorage && aviStorage)
            fileStorage->usePasswordToRead(aviStorage->password());
    }

    return (bool) m_storage;
}

bool QnAviArchiveDelegate::open(
    const QnResourcePtr& resource,
    AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher)
{
    NX_MUTEX_LOCKER lock(&m_openMutex); // need refactor. Now open may be called from UI thread!!!

    m_archiveIntegrityWatcher = archiveIntegrityWatcher;
    m_resource = resource;
    if (m_formatContext == nullptr)
    {
        m_eofReached = false;
        if (!checkStorage())
            return false;

        QString url = m_resource->getUrl();
        if (!m_storage->isFileExists(url))
        {
            using namespace nx::utils::url;
            if (m_storage->getStatus() == nx::vms::api::ResourceStatus::online
                && m_archiveIntegrityWatcher)
            {
                NX_DEBUG(this,
                    "%1: File %2 is missing but the storage %3 is online. Forcing storage reinitialization",
                    __func__, hidePassword(url), hidePassword(m_storage->getUrl()));

                if (const auto initResult = m_storage->initOrUpdate();
                    initResult != nx::vms::api::StorageInitResult::ok)
                {
                    NX_DEBUG(this, "%1: Storage %2 initialization status is %3",
                        __func__, hidePassword(m_storage->getUrl()), initResult);

                    return false;
                }
            }

            if (m_archiveIntegrityWatcher)
                m_archiveIntegrityWatcher->fileMissing(url);

            return false;
        }

        m_formatContext = avformat_alloc_context();
        NX_ASSERT(m_formatContext != nullptr);
        if (m_formatContext == nullptr)
            return false;

        m_IOContext = nx::utils::media::createFfmpegIOContext(m_storage, url, QIODevice::ReadOnly);
        if (!m_IOContext)
        {
            close();
            return false;
        }
        m_formatContext->pb = m_IOContext;
        if (m_beforeOpenInputCallback)
            m_beforeOpenInputCallback(this);
        /**
         * If avformat_open_input() fails, FormatCtx will be freed before avformat_open_input()
         * returns.
         */
        if (avformat_open_input(&m_formatContext, "", 0, 0) < 0)
        {
            close();
            return false;
        }

        if (!initMetadata())
        {
            close();
            return false;
        }
        fillVideoLayout();
    }
    m_keyFrameFound.resize(m_formatContext->nb_streams, false);
    return findStreams();
}

void QnAviArchiveDelegate::close()
{
    if (m_IOContext)
    {
        nx::utils::media::closeFfmpegIOContext(m_IOContext);
        if (m_formatContext)
            m_formatContext->pb = nullptr;
        m_IOContext = nullptr;
    }

    if (m_formatContext)
        avformat_close_input(&m_formatContext);

    m_contexts.clear();
    m_formatContext = nullptr;
    m_streamsFound = false;
    m_storage.clear();
    m_lastPacketTimes.clear();
    m_lastSeekTime = AV_NOPTS_VALUE;
    m_keyFrameFound.clear();
    m_indexToChannel.clear();
}

const char* QnAviArchiveDelegate::getTagValue( const char* tagName )
{
    if (!m_formatContext)
        return 0;
    AVDictionaryEntry* entry = av_dict_get(m_formatContext->metadata, tagName, 0, 0);
    return entry ? entry->value : 0;
}

std::optional<QnAviArchiveMetadata> QnAviArchiveDelegate::metadata() const
{
    return m_metadata;
}

bool QnAviArchiveDelegate::hasVideo() const
{
    auto nonConstThis = const_cast<QnAviArchiveDelegate*> (this);
    nonConstThis->findStreams();
    return m_hasVideo;
}

static QnConstResourceVideoLayoutPtr sDefaultVideoLayout(new QnDefaultResourceVideoLayout());

QnConstResourceVideoLayoutPtr QnAviArchiveDelegate::getVideoLayout()
{
    if (!m_videoLayout)
        return sDefaultVideoLayout;

    return m_videoLayout;
}

void QnAviArchiveDelegate::fillVideoLayout()
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

AudioLayoutConstPtr QnAviArchiveDelegate::getAudioLayout()
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
    if (!m_formatContext)
        return false;

    if (m_streamsFound)
        return true;

    if (m_fastStreamFind) {
        m_formatContext->interrupt_callback.callback = &interruptDetailFindStreamInfo;
        avformat_find_stream_info(m_formatContext, nullptr);
        m_formatContext->interrupt_callback.callback = 0;
        m_streamsFound = m_formatContext->nb_streams > 0;
        for (unsigned i = 0; i < m_formatContext->nb_streams; ++i)
            m_formatContext->streams[i]->first_dts = 0; // reset first_dts. If don't do it, av_seek will seek to begin of file always
    }
    else {
        // TODO: #rvasilenko avoid using deprecated methods
        m_streamsFound = avformat_find_stream_info(m_formatContext, nullptr) >= 0;
    }

    m_firstDts = 0;
    if (m_streamsFound)
    {
        m_durationUs = m_formatContext->duration;
        initLayoutStreams();
        if (m_firstVideoIndex >= 0)
            m_firstDts = m_formatContext->streams[m_firstVideoIndex]->start_time;
        if (m_firstDts == qint64(AV_NOPTS_VALUE))
            m_firstDts = 0;

        if (m_durationUs == AV_NOPTS_VALUE
            && !m_fastStreamFind
            && m_formatContext->nb_streams > 0
            && !m_resource->flags().testFlag(Qn::still_image))
        {
            av_seek_frame(m_formatContext,
                /*stream_index*/ -1,
                /*timestamp*/ std::numeric_limits<qint64>::max(),
                /*flags*/ AVSEEK_FLAG_ANY);
            const auto stream = m_formatContext->streams[0];
            if (stream && stream->cur_dts != AV_NOPTS_VALUE)
            {
                const auto dtsRange = stream->first_dts == AV_NOPTS_VALUE
                    ? stream->cur_dts
                    : stream->cur_dts - stream->first_dts;
                const static AVRational r{1, 1'000'000};
                m_durationUs = av_rescale_q(dtsRange, stream->time_base, r);
            }
            av_seek_frame(m_formatContext, /*stream_index*/ -1, 0, AVSEEK_FLAG_ANY);
        }
    }
    else {
        close();
    }
    return m_streamsFound;
}

void QnAviArchiveDelegate::setForcedVideoChannelsCount(int videoChannels)
{
    m_forceVideoChannelsCount = videoChannels;
}

void QnAviArchiveDelegate::initLayoutStreams()
{
    int lastStreamID = -1;
    m_firstVideoIndex = -1;
    m_hasVideo = false;

    std::set<int> videoStreams;
    std::set<int> audioStreams;
    std::set<int> subtitleStreams;
    for(unsigned i = 0; i < m_formatContext->nb_streams; i++)
    {
        AVStream *strm= m_formatContext->streams[i];
        if(strm->codecpar->codec_type >= AVMEDIA_TYPE_NB)
            continue;

        if (strm->id && strm->id == lastStreamID)
            continue; // duplicate
        lastStreamID = strm->id;

        switch(strm->codecpar->codec_type)
        {
        case AVMEDIA_TYPE_VIDEO:
            if (m_firstVideoIndex == -1)
                m_firstVideoIndex = i;

            // Ffmpeg has a bug when mux AAC into mkv, see
            // https://github.com/HandBrake/HandBrake/issues/2809.
            // So force video channels count from resource video layout.
            if (m_forceVideoChannelsCount.has_value() && m_forceVideoChannelsCount <= videoStreams.size())
                continue;
            videoStreams.insert(i);
            m_hasVideo = true;
            break;
        case AVMEDIA_TYPE_AUDIO:
            audioStreams.insert(i);
            break;

        case AVMEDIA_TYPE_SUBTITLE:
            subtitleStreams.insert(i);
            break;
        default:
            break;
        }
    }
    int channel = 0;
    for (auto& streamIndex: videoStreams)
        m_indexToChannel[streamIndex] = channel++;
    for (auto& streamIndex: audioStreams)
        m_indexToChannel[streamIndex] = channel++;
    for (auto& streamIndex: subtitleStreams)
        m_indexToChannel[streamIndex] = channel++;

    m_audioLayout.reset(new AudioLayout(m_formatContext));
}

bool QnAviArchiveDelegate::initMetadata()
{
    auto aviRes = m_resource.dynamicCast<QnAviResource>();
    if (aviRes && aviRes->hasAviMetadata())
    {
        m_metadata = aviRes->aviMetadata();
        return true;
    }

    m_metadata = QnAviArchiveMetadata::loadFromFile(m_formatContext);
    if (m_archiveIntegrityWatcher
        && !m_archiveIntegrityWatcher->fileRequested(m_metadata, m_resource->getUrl()))
    {
        return false;
    }

    if (aviRes)
        aviRes->setAviMetadata(m_metadata);

    return true;
}

qint64 QnAviArchiveDelegate::packetTimestamp(const AVPacket& packet)
{
    AVStream* stream = m_formatContext->streams[packet.stream_index];
    double timeBase = av_q2d(stream->time_base) * 1'000'000;
    qint64 packetTime = packet.dts != qint64(AV_NOPTS_VALUE) ? packet.dts : packet.pts;
    if (packetTime == qint64(AV_NOPTS_VALUE))
        return AV_NOPTS_VALUE;
    else
        return qMax(0ll, (qint64) (timeBase * (packetTime - m_firstDts))) +  m_startTimeUs;
}

void QnAviArchiveDelegate::packetTimestamp(QnCompressedVideoData* video, const AVPacket& packet)
{
    AVStream* stream = m_formatContext->streams[packet.stream_index];
    double timeBase = av_q2d(stream->time_base) * 1'000'000;
    qint64 packetTime = packet.dts != qint64(AV_NOPTS_VALUE) ? packet.dts : packet.pts;
    if (packetTime == qint64(AV_NOPTS_VALUE))
        video->timestamp = AV_NOPTS_VALUE;
    else
        video->timestamp = qMax(0ll, (qint64) (timeBase * (packetTime - m_firstDts))) +  m_startTimeUs;
    if (packet.pts != AV_NOPTS_VALUE) {
        video->pts = qMax(0ll, (qint64) (timeBase * (packet.pts - m_firstDts))) +  m_startTimeUs;
    }
}

bool QnAviArchiveDelegate::setAudioChannel(unsigned num)
{
    if (!m_formatContext)
        return false;

    if (!m_streamsFound && !findStreams())
        return false;

    // Convert num to absolute track number.
    m_audioStreamIndex = -1;
    int lastStreamID = -1;
    unsigned currentAudioTrackNum = 0;

    for (unsigned i = 0; i < m_formatContext->nb_streams; i++)
    {
        const AVStream* strm = m_formatContext->streams[i];

        if (strm->codecpar->codec_type >= AVMEDIA_TYPE_NB)
            continue;

        if (strm->id && strm->id == lastStreamID)
            continue; //< Duplicate.

        lastStreamID = strm->id;

        if (strm->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            if (currentAudioTrackNum == num)
            {
                m_audioStreamIndex = (int) i;
                m_selectedAudioChannel = num;
                return m_formatContext->streams[m_audioStreamIndex]->codecpar != nullptr;
            }
            currentAudioTrackNum++;
        }
    }
    return false;
}

AVFormatContext* QnAviArchiveDelegate::getFormatContext()
{
    if (!m_streamsFound && !findStreams())
        return nullptr;

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

QIODevice* QnAviArchiveDelegate::ioDevice() const
{
    return m_IOContext ? (QIODevice*) m_IOContext->opaque : nullptr;
}

void QnAviArchiveDelegate::setBeforeOpenInputCallback(BeforeOpenInputCallback callback)
{
    m_beforeOpenInputCallback = callback;
}
