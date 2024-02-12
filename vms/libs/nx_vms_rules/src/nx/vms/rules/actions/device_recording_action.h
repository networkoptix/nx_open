// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/types/resource_types.h>

#include "../basic_action.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API DeviceRecordingAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.deviceRecording")

    FIELD(UuidList, deviceIds, setDeviceIds)
    FIELD(std::chrono::microseconds, interval, setInterval)
    FIELD(nx::vms::api::StreamQuality, quality, setQuality)
    FIELD(int, fps, setFps)
    FIELD(std::chrono::microseconds, duration, setDuration)
    FIELD(std::chrono::microseconds, recordBefore, setRecordBefore)
    FIELD(std::chrono::microseconds, recordAfter, setRecordAfter)

public:
    static const ItemDescriptor& manifest();
};

} // namespace nx::vms::rules
