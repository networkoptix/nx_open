// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "acknowledge_action.h"

#include "../utils/field.h"
#include "../utils/type.h"

using namespace std::chrono_literals;

namespace nx::vms::rules {

const ItemDescriptor& AcknowledgeAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<AcknowledgeAction>(),
        .displayName = tr("Acknowledge"),
        .flags = {ItemFlag::instant, ItemFlag::system},
        .executionTargets = {ExecutionTarget::clients, ExecutionTarget::servers},
        .targetServers = TargetServers::currentServer,
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
