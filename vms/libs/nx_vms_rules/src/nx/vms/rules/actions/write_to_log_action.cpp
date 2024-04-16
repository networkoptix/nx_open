// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "write_to_log_action.h"

#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& WriteToLogAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<WriteToLogAction>(),
        .displayName = tr("Write to Log"),
        .executionTargets = ExecutionTarget::servers,
        .targetServers = TargetServers::currentServer,
        .fields = {
            utils::makeIntervalFieldDescriptor(tr("Interval of Action")),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
