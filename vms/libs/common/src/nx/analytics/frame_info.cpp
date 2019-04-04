#include "frame_info.h"

#include <utility>

namespace nx::analytics {

using namespace std::chrono;

FrameInfo::FrameInfo(int64_t frameTimestampUs):
    m_frameTimestamp(microseconds(frameTimestampUs))
{
}

FrameInfo::FrameInfo(std::chrono::microseconds frameTimestamp):
    m_frameTimestamp(std::move(frameTimestamp))
{
}

std::chrono::microseconds FrameInfo::timestamp() const
{
    return m_frameTimestamp;
}

} // namespace nx::analytics
