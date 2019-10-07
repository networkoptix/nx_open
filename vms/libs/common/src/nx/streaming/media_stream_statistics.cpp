#include "media_stream_statistics.h"
#include <nx/streaming/media_data_packet.h>

static const std::chrono::microseconds kWindowSize = std::chrono::seconds(2);

QnMediaStreamStatistics::QnMediaStreamStatistics()
{
}

void QnMediaStreamStatistics::reset()
{
    QnMutexLocker locker(&m_mutex);
    m_data.clear();
    m_totalSizeBytes = 0;
}

void QnMediaStreamStatistics::onData(
    std::chrono::microseconds timestamp, size_t dataSize, bool isKeyFrame)
{
    auto toIterator =
        [this](std::chrono::microseconds timestamp)
        {
            return std::lower_bound(m_data.begin(), m_data.end(), timestamp);
        };

    auto cleanupRange =
        [this](auto left, auto right)
    {
        if (left >= right)
            return;
        for (auto itr = left; itr != right; ++itr)
            m_totalSizeBytes -= itr->size;
        m_data.erase(left, right);
    };

    QnMutexLocker locker(&m_mutex);
    m_data.insert(toIterator(timestamp), Data{ timestamp, dataSize, isKeyFrame });
    m_totalSizeBytes += dataSize;

    cleanupRange(toIterator(timestamp + kWindowSize), m_data.end());
    cleanupRange(m_data.begin(), toIterator(m_data.back().timestamp - kWindowSize));
}

void QnMediaStreamStatistics::onData(const QnAbstractMediaDataPtr& media)
{
    const auto isKeyFrame = media->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey);
    const auto timestamp = std::chrono::microseconds(media->timestamp);
    return onData(timestamp, media->dataSize(), isKeyFrame);
}

qint64 QnMediaStreamStatistics::bitrateBitsPerSecond() const
{
    QnMutexLocker locker(&m_mutex);
    if (m_data.empty())
        return 0;

    const auto interval = intervalUnsafe().count();
    if (interval > 0)
        return ((m_totalSizeBytes - m_data.rbegin()->size) * 8000000) / interval;
    return 0;
}

bool QnMediaStreamStatistics::hasMediaData() const
{
    QnMutexLocker locker(&m_mutex);
    return m_totalSizeBytes > 0;
}

float QnMediaStreamStatistics::getFrameRate() const
{
    QnMutexLocker locker( &m_mutex );
    if (m_data.empty())
        return 0;
    const auto interval = intervalUnsafe().count();
    if (interval > 0)
        return (m_data.size() - 1) * 1000000.0 / interval;
    return 0;
}

std::chrono::microseconds QnMediaStreamStatistics::intervalUnsafe() const
{
    return m_data.rbegin()->timestamp - m_data.begin()->timestamp;
}

float QnMediaStreamStatistics::getAverageGopSize() const
{
    QnMutexLocker locker(&m_mutex);
    int keyFrames = std::count_if(m_data.begin(), m_data.end(),
        [](const auto& value) { return value.isKeyFrame; });
    return keyFrames > 0 ? m_data.size() / (float) keyFrames : 0;
}
