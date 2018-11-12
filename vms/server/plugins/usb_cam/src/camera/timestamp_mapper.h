#pragma once

#include <map>
#include <optional>

namespace nx {
namespace usb_cam {

class TimestampMapper
{
public:
    void addTimestamp(int64_t ffmpegPts, uint64_t nxTimestamp)
    {
        // Don't allow too many dangling timestamps.
        if (m_timestamps.size() > kTimestampMaxSize)
            m_timestamps.clear();
        m_timestamps.insert(std::pair<int64_t, uint64_t>(ffmpegPts, nxTimestamp));
    }

    std::optional<uint64_t> takeNxTimestamp(int64_t ffmpegPts)
    {
        std::optional<uint64_t> nxTimeStamp;
        auto it = m_timestamps.find(ffmpegPts);
        if (it != m_timestamps.end())
        {
            nxTimeStamp.emplace(it->second);
            m_timestamps.erase(it);
        }
        return nxTimeStamp;
    }

    size_t size() const
    {
        return m_timestamps.size();
    }

    void clear()
    {
        if (m_timestamps.size() > 0)
            m_timestamps.clear();
    }

private:
    std::map<int64_t /*ffmpeg pts*/, uint64_t /*nx timestamp*/> m_timestamps;
    static constexpr int kTimestampMaxSize = 30;
};

} // namespace usb_cam
} // namespace nx