// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_started_event.h"

#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& ServerStartedEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<ServerStartedEvent>(),
        .displayName = tr("Server Started"),
        .description = "",
    };
    return kDescriptor;
}

ServerStartedEvent::ServerStartedEvent(QnUuid serverId, std::chrono::microseconds timestamp):
    base_type(timestamp),
    m_serverId(serverId)
{
}

} // namespace nx::vms::rules
