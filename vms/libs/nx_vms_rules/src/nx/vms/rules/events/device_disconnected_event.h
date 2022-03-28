// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API DeviceDisconnectedEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.deviceDisconnected")
    using base_type = BasicEvent;

    FIELD(QnUuid, deviceId, setDeviceId)

public:
    DeviceDisconnectedEvent(QnUuid deviceId, EventTimestamp timestamp);

    static const ItemDescriptor& manifest();
};

} // namespace nx::vms::rules
