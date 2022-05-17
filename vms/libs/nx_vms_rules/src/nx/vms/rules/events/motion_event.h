// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API MotionEvent: public BasicEvent
{
    Q_OBJECT
    using base_type = BasicEvent;
    Q_CLASSINFO("type", "nx.events.motion")

    FIELD(QnUuid, source, setSource)

public:
    static const ItemDescriptor& manifest();

    MotionEvent() = default;
    MotionEvent(std::chrono::microseconds timestamp, State state, QnUuid deviceId);
};

} // namespace nx::vms::rules
