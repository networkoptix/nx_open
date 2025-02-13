// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <chrono>

#include <nx/media/media_data_packet.h>

// This class was supposed to eliminate the gaps and recalculate the timestamps from 0 or offset,
// individually for each stream.
class NX_VMS_COMMON_API TimestampCorrector
{
public:
    TimestampCorrector(
        std::chrono::microseconds maxFrameDurationUs,
        std::chrono::microseconds defaultFrameDurationUs);
    void setOffset(std::chrono::microseconds value);

    std::chrono::microseconds process(
        std::chrono::microseconds timestamp, int streamIndex, bool equalAllowed);

    void clear();

private:
    const std::chrono::microseconds kMaxFrameDurationUs;
    const std::chrono::microseconds kDefaultFrameDurationUs;
    std::chrono::microseconds m_offset{};
    std::optional<std::chrono::microseconds> m_baseTime;
    std::map<int, std::chrono::microseconds> m_lastPacketTimestamps; //< By stream index.
};
