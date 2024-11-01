// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/rules/basic_action.h>

#include "../rules_fwd.h"

namespace nx::vms::rules {

inline bool isProlonged(const ActionPtr& action)
{
    const auto actionState = action->state();
    return actionState == State::started || actionState == State::stopped;
}

/** Returns whether the action is prolonged and has no fixed duration. */
NX_VMS_RULES_API bool isProlonged(const Engine* engine, const ActionBuilder* builder);

/** Returns whether the action is prolonged only and cannot be connected to instant only event. */
NX_VMS_RULES_API bool hasDuration(const vms::rules::ItemDescriptor& actionDescriptor);

NX_VMS_RULES_API bool hasTargetCamera(const vms::rules::ItemDescriptor& actionDescriptor);

NX_VMS_RULES_API bool hasTargetLayout(const vms::rules::ItemDescriptor& actionDescriptor);

NX_VMS_RULES_API bool hasTargetUser(const vms::rules::ItemDescriptor& actionDescriptor);

NX_VMS_RULES_API bool hasTargetServer(const vms::rules::ItemDescriptor& actionDescriptor);

NX_VMS_RULES_API bool needAcknowledge(const ActionPtr& action);

/** Returns possible states that might be used in event filter. */
NX_VMS_RULES_API QSet<State> getPossibleFilterStatesForActionDescriptor(
    const vms::rules::ItemDescriptor& actionDescriptor);

/** Returns possible states that might be used with the given action builder. */
NX_VMS_RULES_API QSet<State> getPossibleFilterStatesForActionBuilder(
    const Engine* engine, const ActionBuilder* actionBuilder);

} // namespace nx::vms::rules
