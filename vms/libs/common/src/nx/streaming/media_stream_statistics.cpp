#include "media_stream_statistics.h"
#include <nx/streaming/media_data_packet.h>

static const int kMaxErrorsToReportIssue = 2;
static const std::chrono::microseconds kWindowSize = std::chrono::seconds(2);

QnMediaStreamStatistics::QnMediaStreamStatistics()
{
}

void QnMediaStreamStatistics::reset()
{
    QnMutexLocker locker(&m_mutex);
    m_data.clear();
    m_numberOfErrors = 0;
    m_totalSizeBytes = 0;
}

void QnMediaStreamStatistics::onData(
    std::chrono::microseconds timestamp, size_t dataSize, std::optional<bool> isKeyFrame)
{
    QnMutexLocker locker(&m_mutex);
    auto itr = std::lower_bound(m_data.begin(), m_data.end(), timestamp);
    m_data.insert(itr, Data{ timestamp, dataSize, std::optional<bool>(isKeyFrame) });
    m_totalSizeBytes += dataSize;

    auto timeThreshold = m_data.rbegin()->timestamp - kWindowSize;
    auto rightItr = std::lower_bound(m_data.begin(), m_data.end(), timeThreshold);
    for (auto itr = m_data.begin(); itr < rightItr; ++itr)
        m_totalSizeBytes -= itr->size;
    m_data.erase(m_data.begin(), rightItr);
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

    const auto interval = (m_data.rbegin()->timestamp - m_data.begin()->timestamp).count();
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
    const auto interval = (m_data.rbegin()->timestamp - m_data.begin()->timestamp).count();
    if (interval > 0)
        return (m_data.size() - 1) * 1000000.0 / interval;
    return 0;
}

float QnMediaStreamStatistics::getAverageGopSize() const
{
    QnMutexLocker locker(&m_mutex);
    int totalFrames = 0;
    int keyFrames = 0;
    for (const auto& value: m_data)
    {
        if (value.isKeyFrame.has_value())
        {
            ++totalFrames;
            if (*value.isKeyFrame)
                ++keyFrames;
        }
    }
    return keyFrames > 0 ? totalFrames / (float) keyFrames : 0;
}

void QnMediaStreamStatistics::onEvent(
    std::chrono::microseconds timestamp,
    CameraDiagnostics::Result event)
{
    onData(timestamp, /*size*/ 0);

    QnMutexLocker locker(&m_mutex);
    updateStatisticsUnsafe(event);
}

void QnMediaStreamStatistics::updateStatisticsUnsafe(CameraDiagnostics::Result event)
{
    if (event.errorCode == CameraDiagnostics::ErrorCode::noError)
    {
        if (m_numberOfErrors > 0)
            emit streamEvent(event); //< Report no error.
        m_numberOfErrors = 0;
    }
    else
    {
        ++m_numberOfErrors;
        if (m_numberOfErrors % kMaxErrorsToReportIssue == 0)
            emit streamEvent(event);
    }
}

bool QnMediaStreamStatistics::isConnectionLost() const
{
    return m_numberOfErrors >= kMaxErrorsToReportIssue;
}
