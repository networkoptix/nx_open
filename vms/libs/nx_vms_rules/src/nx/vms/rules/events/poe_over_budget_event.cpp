// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "poe_over_budget_event.h"

#include "../event_fields/source_server_field.h"

namespace nx::vms::rules {

const ItemDescriptor& PoeOverBudgetEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = eventType<PoeOverBudgetEvent>(),
        .displayName = tr("PoE over Budget"),
        .description = "",
        .fields = {
            makeFieldDescriptor<SourceServerField>("serverId", tr("Server")),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
