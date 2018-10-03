#pragma once

#include <map>

namespace nx {
namespace usb_cam {

class TimestampMapper
{
public:
    void addTimestamp(int64_t ffmpegPts, uint64_t nxTimestamp)
    {
        m_timestamps.insert(std::pair<int64_t, uint64_t>(ffmpegPts, nxTimestamp));
    }

    bool getNxTimestamp(int64_t ffmpegPts, uint64_t * outNxTimestamp, bool eraseEntry = true)
    {
        *outNxTimestamp = 0;
        auto it = m_timestamps.find(ffmpegPts);
        bool found = it != m_timestamps.end();
        if (found)
        {
            *outNxTimestamp = it->second;
            if (eraseEntry)
                m_timestamps.erase(it);
        }
        return found;
    }

    size_t size() const
    {
        return m_timestamps.size();
    }

    void clear()
    {
        m_timestamps.clear();
    }

private:
    std::map<int64_t /*ffmpeg pts*/, uint64_t /*nx timestamp*/> m_timestamps;
};

} // namespace usb_cam
} // namespace nx