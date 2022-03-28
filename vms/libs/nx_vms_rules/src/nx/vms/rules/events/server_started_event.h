// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API ServerStartedEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.serverStarted")
    using base_type = BasicEvent;

    FIELD(QnUuid, serverId, setServerId)


public:
    static const ItemDescriptor& manifest();

    ServerStartedEvent(QnUuid serverId, EventTimestamp timestamp);
};

} // namespace nx::vms::rules
