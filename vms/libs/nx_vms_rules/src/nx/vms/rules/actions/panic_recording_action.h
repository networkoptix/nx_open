// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_action.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API PanicRecordingAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.panicRecording")

    FIELD(std::chrono::microseconds, duration, setDuration)
    FIELD(std::chrono::microseconds, interval, setInterval)

public:
    static const ItemDescriptor& manifest();

    PanicRecordingAction() = default;
};

} // namespace nx::vms::rules
