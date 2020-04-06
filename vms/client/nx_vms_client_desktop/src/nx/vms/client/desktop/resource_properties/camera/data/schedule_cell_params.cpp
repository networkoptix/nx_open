#include "schedule_cell_params.h"

#include <nx/utils/math/fuzzy.h>

namespace nx::vms::client::desktop {

const ScheduleCellParams ScheduleCellParams::kEmptyValue =
    {0, Qn::StreamQuality::undefined, Qn::RecordingType::never, kAutomaticBitrate};

const ScheduleCellParams ScheduleCellParams::kUnsetValue =
    {-1, Qn::StreamQuality::undefined, Qn::RecordingType::never, kAutomaticBitrate};

bool ScheduleCellParams::operator==(const ScheduleCellParams& other) const
{
    return fps == other.fps
        && quality == other.quality
        && recordingType == other.recordingType
        && qFuzzyEquals(bitrateMbps, other.bitrateMbps);
}

bool ScheduleCellParams::isAutomaticBitrate() const
{
    return qFuzzyEquals(bitrateMbps, kAutomaticBitrate);
}

} // namespace nx::vms::client::desktop
