#pragma once

#include <common/common_globals.h>

namespace nx::vms::client::desktop {

struct ScheduleCellParams
{
    static constexpr auto kAutomaticBitrate = 0.0;
    static constexpr auto kDefaultRecordingType = Qn::RecordingType::always;

    static const ScheduleCellParams kEmptyValue;
    static const ScheduleCellParams kUnsetValue;

    int fps = 0;
    Qn::StreamQuality quality = Qn::StreamQuality::normal;
    Qn::RecordingType recordingType = kDefaultRecordingType;
    qreal bitrateMbps = kAutomaticBitrate;

    bool operator==(const ScheduleCellParams& other) const;
    bool isAutomaticBitrate() const;
};

} // namespace nx::vms::client::desktop
