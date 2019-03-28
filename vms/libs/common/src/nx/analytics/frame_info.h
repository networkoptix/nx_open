#pragma once

#include <nx/analytics/i_frame_info.h>

namespace nx::analytics {

class FrameInfo: public IFrameInfo
{
public:
    FrameInfo(int64_t frameTimestampUs);
    FrameInfo(std::chrono::microseconds frameTimestamp);

    virtual std::chrono::microseconds timestamp() const override;

private:
    std::chrono::microseconds m_frameTimestamp{0};
};

} // namespace nx::analytics
