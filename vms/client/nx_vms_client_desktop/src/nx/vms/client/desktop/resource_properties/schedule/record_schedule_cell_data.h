// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>
#include <nx/vms/api/types/resource_types.h>

namespace nx::vms::client::desktop {

struct NX_VMS_CLIENT_DESKTOP_API RecordScheduleCellData
{
    static constexpr auto kAutoBitrateValue = 0.0;

    // Must be in sync with nx::vms::api::ScheduleTaskData default value.
    int fps = 0;

    // Must be in sync with nx::vms::api::ScheduleTaskData default value.
    nx::vms::api::StreamQuality streamQuality = nx::vms::api::StreamQuality::undefined;

    // Must be in sync with nx::vms::api::ScheduleTaskData default value.
    nx::vms::api::RecordingType recordingType = nx::vms::api::RecordingType::always;

    // Must be in sync with nx::vms::api::ScheduleTaskData default value.
    nx::vms::api::RecordingMetadataTypes metadataTypes{};

    double bitrateMbitPerSec = kAutoBitrateValue; //< TODO: Replace with 'int bitrateKbitPerSec'.

    bool isAutoBitrate() const;

    bool operator==(const RecordScheduleCellData& other) const;
    bool operator<(const RecordScheduleCellData& other) const;
};

NX_REFLECTION_INSTRUMENT(RecordScheduleCellData,
    (fps)(streamQuality)(recordingType)(metadataTypes)(bitrateMbitPerSec))

} // namespace nx::vms::client::desktop
