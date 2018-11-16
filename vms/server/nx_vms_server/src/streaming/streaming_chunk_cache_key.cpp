#include "streaming_chunk_cache_key.h"

#include <QHash>
#include <QStringList>

#include "streaming_params.h"

StreamingChunkCacheKey::StreamingChunkCacheKey():
    m_channel(0),
    m_startTimestamp(0),
    m_duration(0)
{
}

StreamingChunkCacheKey::StreamingChunkCacheKey(
    const QString& uniqueResourceID,
    int channel,
    const QString& containerFormat,
    const QString& alias,
    quint64 startTimestamp,
    std::chrono::microseconds duration,
    MediaQuality streamQuality,
    const std::multimap<QString, QString>& auxiliaryParams)
    :
    m_uniqueResourceID(uniqueResourceID),
    m_channel(channel),
    m_containerFormat(containerFormat),
    m_alias(alias),
    m_startTimestamp(startTimestamp),
    m_duration(duration),
    m_streamQuality(streamQuality),
    m_isLive(false),
    m_auxiliaryParams(auxiliaryParams)
{
    NX_ASSERT(!containerFormat.isEmpty());

    std::multimap<QString, QString>::const_iterator it = auxiliaryParams.find(StreamingParams::VIDEO_CODEC_PARAM_NAME);
    if (it != auxiliaryParams.end())
        m_videoCodec = it->second;

    it = auxiliaryParams.find(StreamingParams::PICTURE_SIZE_PIXELS_PARAM_NAME);
    if (it != auxiliaryParams.end())
    {
        //1920x1080
        const QStringList& sizesStr = it->second.split(QChar('x'));
        m_pictureSizePixels = QSize(
            sizesStr.size() > 0 ? sizesStr[0].toInt() : 0,
            sizesStr.size() > 1 ? sizesStr[1].toInt() : 0);
    }

    it = auxiliaryParams.find(StreamingParams::LIVE_PARAM_NAME);
    m_isLive = it != auxiliaryParams.end();
}

StreamingChunkCacheKey::StreamingChunkCacheKey(const QString& uniqueResourceID):
    m_uniqueResourceID(uniqueResourceID),
    m_channel(0),
    m_startTimestamp(0),
    m_duration(0)
{
}

//!data source (camera id, stream id)
const QString& StreamingChunkCacheKey::srcResourceUniqueID() const
{
    return m_uniqueResourceID;
}

unsigned int StreamingChunkCacheKey::channel() const
{
    return m_channel;
}

QString StreamingChunkCacheKey::alias() const
{
    return m_alias;
}

quint64 StreamingChunkCacheKey::startTimestamp() const
{
    return m_startTimestamp;
}

std::chrono::microseconds StreamingChunkCacheKey::duration() const
{
    return m_duration;
}

quint64 StreamingChunkCacheKey::endTimestamp() const
{
    return m_startTimestamp + m_duration.count();
}

MediaQuality StreamingChunkCacheKey::streamQuality() const
{
    return m_streamQuality;
}

const QSize& StreamingChunkCacheKey::pictureSizePixels() const
{
    return m_pictureSizePixels;
}

const QString& StreamingChunkCacheKey::containerFormat() const
{
    return m_containerFormat;
}

const QString& StreamingChunkCacheKey::videoCodec() const
{
    return m_videoCodec;
}

AVCodecID StreamingChunkCacheKey::audioCodecId() const
{
    return m_audioCodecId;
}

void StreamingChunkCacheKey::setAudioCodecId(AVCodecID audioCodecId)
{
    m_audioCodecId = audioCodecId;
}

bool StreamingChunkCacheKey::live() const
{
    return m_isLive;
}

QString StreamingChunkCacheKey::streamingSessionId() const
{
    auto it = m_auxiliaryParams.find(StreamingParams::SESSION_ID_PARAM_NAME);
    if (it != m_auxiliaryParams.end())
        return it->second;
    return QString();
}

bool StreamingChunkCacheKey::mediaStreamParamsEqualTo(
    const StreamingChunkCacheKey& right) const
{
    return srcResourceUniqueID() == right.srcResourceUniqueID() &&
        channel() == right.channel() &&
        streamQuality() == right.streamQuality() &&
        pictureSizePixels() == right.pictureSizePixels() &&
        containerFormat() == right.containerFormat() &&
        videoCodec() == right.videoCodec() &&
        audioCodecId() == right.audioCodecId();
}

bool StreamingChunkCacheKey::operator<(
    const StreamingChunkCacheKey& right) const
{
    if (m_uniqueResourceID != right.m_uniqueResourceID)
        return m_uniqueResourceID < right.m_uniqueResourceID;

    if (m_channel != right.m_channel)
        return m_channel < right.m_channel;

    if (m_containerFormat != right.m_containerFormat)
        return m_containerFormat < right.m_containerFormat;

    if (m_alias != right.m_alias)
        return m_alias < right.m_alias;

    if (m_startTimestamp != right.m_startTimestamp)
        return m_startTimestamp < right.m_startTimestamp;

    if (m_duration != right.m_duration)
        return m_duration < right.m_duration;

    if (m_pictureSizePixels.width() != right.m_pictureSizePixels.width())
        return m_pictureSizePixels.width() < right.m_pictureSizePixels.width();

    if (m_pictureSizePixels.height() != right.m_pictureSizePixels.height())
        return m_pictureSizePixels.height() < right.m_pictureSizePixels.height();

    if (m_videoCodec != right.m_videoCodec)
        return m_videoCodec < right.m_videoCodec;

    if (m_audioCodecId != right.m_audioCodecId)
        return m_audioCodecId < right.m_audioCodecId;

    if (m_streamQuality != right.m_streamQuality)
        return m_streamQuality < right.m_streamQuality;

    return false;
}

bool StreamingChunkCacheKey::operator>(const StreamingChunkCacheKey& right) const
{
    return right < *this;
}

bool StreamingChunkCacheKey::operator==(const StreamingChunkCacheKey& right) const
{
    return (m_uniqueResourceID == right.m_uniqueResourceID)
        && (m_channel == right.m_channel)
        && (m_containerFormat == right.m_containerFormat)
        && (m_alias == right.m_alias)
        && (m_startTimestamp == right.m_startTimestamp)
        && (m_duration == right.m_duration)
        && (m_streamQuality == right.m_streamQuality);
}

bool StreamingChunkCacheKey::operator!=(const StreamingChunkCacheKey& right) const
{
    return !(*this == right);
}

uint qHash(const StreamingChunkCacheKey& key)
{
    return qHash(key.srcResourceUniqueID())
        + key.channel()
        + key.startTimestamp()
        + qHash(key.alias())
        + key.duration().count()
        + key.endTimestamp()
        + key.streamQuality()
        + key.pictureSizePixels().width()
        + key.pictureSizePixels().height()
        + qHash(key.containerFormat())
        + qHash(key.videoCodec())
        + qHash(key.audioCodecId());
}
