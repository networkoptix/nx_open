#pragma once

#include <common/common_globals.h>

#include <nx/utils/math/fuzzy.h>

namespace nx::vms::client::desktop {

struct ScheduleCellParams
{
    static constexpr auto kAutomaticBitrate = 0.0;
    static constexpr auto kDefaultFps = 10;
    static constexpr auto kDefaultRecordingType = Qn::RecordingType::always;

    int fps = kDefaultFps;
    Qn::StreamQuality quality = Qn::StreamQuality::normal;
    Qn::RecordingType recordingType = kDefaultRecordingType;
    qreal bitrateMbps = kAutomaticBitrate;

    bool operator==(const ScheduleCellParams& other) const
    {
        return fps == other.fps
            && quality == other.quality
            && recordingType == other.recordingType
            && qFuzzyEquals(bitrateMbps, other.bitrateMbps);
    }

    bool isAutomaticBitrate() const
    {
        return qFuzzyEquals(bitrateMbps, kAutomaticBitrate);
    }
};

} // namespace nx::vms::client::desktop
