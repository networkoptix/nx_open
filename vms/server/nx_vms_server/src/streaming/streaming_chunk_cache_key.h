#pragma once

#include <chrono>
#include <map>

#include <QDateTime>
#include <QSize>
#include <QString>
#include <set>

#include <nx/streaming/media_data_packet.h>

/**
 * Identifies chunk in cache by following parameters:
 * - id of resource
 * - start/end position
 * - media stream format
 * - container format
 * - video codec type
 * - video resolution
 */
class StreamingChunkCacheKey
{
public:
    StreamingChunkCacheKey();
    /**
     * @param channel channel number or -1 for all channels
     * @param containerFormat E.g., ts for mpeg2/ts
     * @param startTimestamp In micros
     * @param duration In micros
     * @param streamQuality MEDIA_Quality_High or MEDIA_Quality_Low
     */
    StreamingChunkCacheKey(
        const QString& uniqueResourceID,
        int channel,
        const QString& containerFormat,
        const QString& alias,
        quint64 startTimestamp,
        std::chrono::microseconds duration,
        MediaQuality streamQuality,
        const std::multimap<QString, QString>& auxiliaryParams);
    StreamingChunkCacheKey(const QString& uniqueResourceID);

    const QString& srcResourceUniqueID() const;
    unsigned int channel() const;
    QString alias() const;
    /** Chunk start timestamp (micros). This is internal timestamp, not calendar time. */
    quint64 startTimestamp() const;
    std::chrono::microseconds duration() const;
    quint64 endTimestamp() const;
    MediaQuality streamQuality() const;
    /**
     * @return If no resolution specified, returns invalid QSize.
     */
    const QSize& pictureSizePixels() const;
    /** Media format (codec format, container format). */
    const QString& containerFormat() const;

    /**
     * @return list of allowed video codecs. If source video has codec from the list
     * it will be not transcoded. Otherwise it is transcoded to the first codec of the list.
     */
    std::vector<AVCodecID> supportedVideoCodecs() const;

    AVCodecID audioCodecId() const;
    void setAudioCodecId(AVCodecID);

    QString streamingSessionId() const;

    bool live() const;

    bool mediaStreamParamsEqualTo(const StreamingChunkCacheKey& right) const;

    bool operator<(const StreamingChunkCacheKey& right) const;
    bool operator>(const StreamingChunkCacheKey& right) const;
    bool operator==(const StreamingChunkCacheKey& right) const;
    bool operator!=(const StreamingChunkCacheKey& right) const;

private:
    QString m_uniqueResourceID;
    int m_channel;
    QString m_containerFormat;
    QString m_alias;
    quint64 m_startTimestamp;
    std::chrono::microseconds m_duration;
    MediaQuality m_streamQuality;
    bool m_isLive;
    QSize m_pictureSizePixels;
    std::vector<AVCodecID> m_videoCodecs;
    AVCodecID m_audioCodecId = AV_CODEC_ID_NONE;
    std::multimap<QString, QString> m_auxiliaryParams;
};

uint qHash(const StreamingChunkCacheKey& key);
