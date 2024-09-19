// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "write_to_log_action.h"

#include "../strings.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& WriteToLogAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<WriteToLogAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Write to Log")),
        .description = "Write a record to the site's log.",
        .executionTargets = ExecutionTarget::servers,
        .targetServers = TargetServers::currentServer,
        .fields = {
            utils::makeIntervalFieldDescriptor(Strings::intervalOfAction()),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
