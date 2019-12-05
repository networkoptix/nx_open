#pragma once

#include <optional>
#include <mutex>
#include <deque>
#include <chrono>

namespace nx {
namespace sdk {

class MediaStreamStatistics
{
public:
    MediaStreamStatistics();

    void reset();
    void onData(std::chrono::microseconds timestamp, size_t dataSize, bool isKeyFrame);
    int64_t bitrateBitsPerSecond() const;
    float getFrameRate() const;
    float getAverageGopSize() const;
    bool hasMediaData() const;

private:
    struct Data
    {
        std::chrono::microseconds timestamp{};
        size_t size = 0;
        bool isKeyFrame = false;

        bool operator<(std::chrono::microseconds value) const { return timestamp < value; }
    };

    std::chrono::microseconds intervalUnsafe() const;

    mutable std::mutex m_mutex;
    std::deque<Data> m_data;
    int64_t m_totalSizeBytes = 0;
    //nx::utils::ElapsedTimer m_lastDataTimer;
    std::chrono::steady_clock::time_point m_lastDataTimer;
};

} // namespace sdk
} // namespace nx
