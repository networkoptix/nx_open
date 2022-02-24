// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

namespace nx::recording::helpers {

/** Removes gaps between timestamps lasting more than 1 second. */
class NX_VMS_COMMON_API TimestampStitcher
{
public:
    std::chrono::microseconds process(std::chrono::microseconds timestamp);

private:
    static constexpr auto kDefaultFrameDuration = std::chrono::milliseconds(33);

private:
    bool m_firstFrame = true;
    std::chrono::microseconds m_duration{kDefaultFrameDuration};
    std::chrono::microseconds m_lastOutputTimestamp{0};
    std::chrono::microseconds m_lastInputTimestamp{0};
};

} // namespace nx::recording::helpers
