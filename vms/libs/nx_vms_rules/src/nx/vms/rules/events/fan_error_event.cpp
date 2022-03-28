// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fan_error_event.h"

#include "../event_fields/source_server_field.h"

namespace nx::vms::rules {

const ItemDescriptor& FanErrorEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = eventType<FanErrorEvent>(),
        .displayName = tr("Fan Error"),
        .description = "",
        .fields = {
            makeFieldDescriptor<SourceServerField>("serverId", tr("Server")),
        }
    };
    return kDescriptor;
}

FanErrorEvent::FanErrorEvent(QnUuid serverId, EventTimestamp timestamp):
    base_type(timestamp),
    m_serverId(serverId)
{
}

} // namespace nx::vms::rules
