#pragma once

#include <map>

namespace nx {
namespace usb_cam {

class TimeStampMapper
{
public:
    void addTimeStamp(int64_t ffmpegPts, uint64_t nxTimeStamp)
    {
        m_timeStamps.insert(std::pair<int64_t, uint64_t>(ffmpegPts, nxTimeStamp));
    }

    bool getNxTimeStamp(int64_t ffmpegPts, uint64_t * outNxTimeStamp, bool eraseEntry = true)
    {
        *outNxTimeStamp = 0;
        auto it = m_timeStamps.find(ffmpegPts);
        bool found = it != m_timeStamps.end();
        if (found)
        {
            *outNxTimeStamp = it->second;
            if (eraseEntry)
                m_timeStamps.erase(it);
        }
        return found;
    }

private:
    std::map<int64_t /*ffmpeg pts*/, uint64_t /*nx timeStamp*/> m_timeStamps;
};

} // namespace usb_cam
} // namespace nx