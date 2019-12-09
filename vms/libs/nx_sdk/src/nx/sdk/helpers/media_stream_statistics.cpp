#include "media_stream_statistics.h"

#include <algorithm>

static const std::chrono::microseconds kWindowSize = std::chrono::seconds(2);

namespace nx {
namespace sdk {

using namespace std::chrono;

MediaStreamStatistics::MediaStreamStatistics()
{
    reset();
}

void MediaStreamStatistics::reset()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    m_data.clear();
    m_totalSizeBytes = 0;
    m_lastDataTimer = steady_clock::now();
}

void MediaStreamStatistics::onData(
    microseconds timestamp, size_t dataSize, bool isKeyFrame)
{
    auto toIterator =
        [this](microseconds timestamp)
        {
            return std::lower_bound(m_data.begin(), m_data.end(), timestamp);
        };

    auto removeRange =
        [this](auto left, auto right)
        {
            if (left >= right)
                return;
            for (auto itr = left; itr != right; ++itr)
                m_totalSizeBytes -= itr->size;
            m_data.erase(left, right);
        };

    std::lock_guard<std::mutex> locker(m_mutex);
    m_data.insert(toIterator(timestamp), Data(timestamp, dataSize, isKeyFrame));
    m_totalSizeBytes += dataSize;

    // Remove old and future data in case of media stream time has been changed.
    removeRange(toIterator(timestamp + kWindowSize), m_data.end());
    removeRange(m_data.begin(), toIterator(m_data.back().timestamp - kWindowSize));
    m_lastDataTimer = steady_clock::now();
}

int64_t MediaStreamStatistics::bitrateBitsPerSecond() const
{
    std::lock_guard<std::mutex> locker(m_mutex);
    const auto elapsed = steady_clock::now() - m_lastDataTimer;
    if (m_data.empty() || elapsed > kWindowSize)
        return 0;

    const auto interval = intervalUnsafe().count();
    if (interval > 0)
        return ((m_totalSizeBytes - m_data.rbegin()->size) * 8000000) / interval;
    return 0;
}

bool MediaStreamStatistics::hasMediaData() const
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_totalSizeBytes > 0;
}

float MediaStreamStatistics::getFrameRate() const
{
    std::lock_guard<std::mutex> locker(m_mutex);
    const auto elapsed = steady_clock::now() - m_lastDataTimer;
    if (m_data.empty() || elapsed > kWindowSize)
        return 0;
    const auto interval = intervalUnsafe().count();
    if (interval > 0)
        return (m_data.size() - 1) * 1000000.0F / interval;
    return 0;
}

microseconds MediaStreamStatistics::intervalUnsafe() const
{
    return m_data.rbegin()->timestamp - m_data.begin()->timestamp;
}

float MediaStreamStatistics::getAverageGopSize() const
{
    std::lock_guard<std::mutex> locker(m_mutex);
    const size_t keyFrames = std::count_if(m_data.begin(), m_data.end(),
        [](const auto& value) { return value.isKeyFrame; });
    return keyFrames > 0 ? m_data.size() / (float) keyFrames : 0;
}

} // namespace sdk
} // namespace nx
