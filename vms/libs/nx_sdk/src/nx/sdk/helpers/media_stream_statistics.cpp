// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_stream_statistics.h"

#include <algorithm>

namespace nx::sdk {

using namespace std::chrono;

MediaStreamStatistics::MediaStreamStatistics(
    std::chrono::microseconds windowSize,
    int maxDurationInFrames)
    :
    m_windowSize(windowSize),
    m_maxDurationInFrames(maxDurationInFrames)
{
    reset();
}

void MediaStreamStatistics::setWindowSize(std::chrono::microseconds windowSize)
{
    m_windowSize = windowSize;
}

void MediaStreamStatistics::setMaxDurationInFrames(int maxDurationInFrames)
{
    m_maxDurationInFrames = maxDurationInFrames;
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
            for (auto itr = left; itr != right; ++itr)
                m_totalSizeBytes -= itr->size;
            m_data.erase(left, right);
        };

    std::lock_guard<std::mutex> locker(m_mutex);
    m_data.insert(toIterator(timestamp), Data(timestamp, dataSize, isKeyFrame));
    m_totalSizeBytes += dataSize;

    // Remove old and future data in case of media stream time has been changed.
    removeRange(toIterator(timestamp + m_windowSize), m_data.end());
    if (!m_data.empty())
        removeRange(m_data.begin(), toIterator(m_data.back().timestamp - m_windowSize));
    const int extraFrames = (int) m_data.size() - m_maxDurationInFrames;
    if (extraFrames > 0 && m_maxDurationInFrames > 0)
        removeRange(m_data.begin(), m_data.begin() + extraFrames);
    m_lastDataTimer = steady_clock::now();
}

int64_t MediaStreamStatistics::bitrateBitsPerSecond() const
{
    std::lock_guard<std::mutex> locker(m_mutex);
    const auto elapsed = steady_clock::now() - m_lastDataTimer;
    if (m_data.empty() || elapsed > m_windowSize)
        return 0;

    const auto interval = intervalUnsafe().count();
    if (interval > 0)
        return ((m_totalSizeBytes - m_data.rbegin()->size) * 8'000'000) / interval;
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
    if (m_data.empty() || elapsed > m_windowSize)
        return 0;
    const auto interval = intervalUnsafe().count();
    if (interval > 0)
        return (m_data.size() - 1) * 1'000'000.0F / interval;
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

} // namespace nx::sdk
