// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>
#include <nx/vms/event/event_fwd.h>

namespace nx::vms::common { class SystemContext; }

namespace nx {
namespace vms {
namespace event {

class AggregationInfo;

class NX_VMS_COMMON_API ActionFactory
{
public:
    static AbstractActionPtr instantiateAction(
        nx::vms::common::SystemContext* systemContext,
        const RulePtr& rule,
        const AbstractEventPtr& event,
        const nx::Uuid& moduleGuid,
        EventState state);

    static AbstractActionPtr instantiateAction(
        nx::vms::common::SystemContext* systemContext,
        const RulePtr& rule,
        const AbstractEventPtr& event,
        const nx::Uuid& moduleGuid,
        const AggregationInfo& aggregationInfo);

    static AbstractActionPtr createAction(const ActionType actionType,
        const EventParameters& runtimeParams);

    static AbstractActionPtr cloneAction(AbstractActionPtr action);
};

} // namespace event
} // namespace vms
} // namespace nx
