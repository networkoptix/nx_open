// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "record_schedule_cell_data.h"

#include <nx/utils/math/fuzzy.h>
#include <tuple>

namespace nx::vms::client::desktop {

bool RecordScheduleCellData::operator==(const RecordScheduleCellData& other) const
{
    return fps == other.fps
        && streamQuality == other.streamQuality
        && recordingType == other.recordingType
        && metadataTypes == other.metadataTypes
        && qFuzzyEquals(bitrateMbitPerSec, other.bitrateMbitPerSec);
}

bool RecordScheduleCellData::operator<(const RecordScheduleCellData& other) const
{
    int bitrateCompare = 0;
    if (!qFuzzyEquals(bitrateMbitPerSec, other.bitrateMbitPerSec))
        bitrateCompare = bitrateMbitPerSec < other.bitrateMbitPerSec ? -1 : 1;

    return (bitrateCompare == 0 && std::tie(fps, streamQuality, recordingType, metadataTypes) <
        std::tie(other.fps, other.streamQuality, other.recordingType, other.metadataTypes))
            || bitrateCompare < 0;
}

bool RecordScheduleCellData::isAutoBitrate() const
{
    return qFuzzyEquals(bitrateMbitPerSec, kAutoBitrateValue);
}

} // namespace nx::vms::client::desktop
