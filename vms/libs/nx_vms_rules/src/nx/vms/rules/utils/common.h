// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

#include "../manifest.h"
#include "../rules_fwd.h"

namespace nx::vms::common { class SystemContext; }
namespace nx::vms::rules { class StateField; }

namespace nx::vms::rules::utils {

inline bool aggregateByType(
    const ItemDescriptor& eventDescriptor,
    const ItemDescriptor& actionDescriptor)
{
    return actionDescriptor.flags.testFlag(ItemFlag::aggregationByTypeSupported)
        && eventDescriptor.flags.testFlag(ItemFlag::aggregationByTypeSupported);
}

inline bool isInstantOnly(const ItemDescriptor& descriptor)
{
    return descriptor.flags.testFlag(vms::rules::ItemFlag::instant)
        && !descriptor.flags.testFlags(vms::rules::ItemFlag::prolonged);
}

inline bool isProlongedOnly(const ItemDescriptor& descriptor)
{
    return descriptor.flags.testFlag(vms::rules::ItemFlag::prolonged)
        && !descriptor.flags.testFlags(vms::rules::ItemFlag::instant);
}

/**
 * Returns field descriptor with the given field name from the given field descriptor or
 * std::nullopt if there is no such field.
 */
NX_VMS_RULES_API std::optional<FieldDescriptor> fieldByName(
    const QString& fieldName, const ItemDescriptor& descriptor);

/** Returns whether logging is allowed for the given rule. */
NX_VMS_RULES_API bool isLoggingAllowed(const Engine* engine, nx::Uuid ruleId);

/**
 * Return whether the given event filter is compatible with the given action builder.
 * The function checks whether selected event and action types are compatible and whether event
 * state in the event filter is compatible for the given event type and for the given action builder.
 */
NX_VMS_RULES_API bool isCompatible(
    const Engine* engine,
    const EventFilter* eventFilter,
    const ActionBuilder* actionBuilder);

/**
 * Returns sorted list of all the available states that can be used by the StateField with the given
 * rule. These states are the intersection of the states available for the event filter and action
 * builder in the given rule.
 */
NX_VMS_RULES_API QList<State> getAvailableStates(const Engine* engine, const Rule* rule);

} // namespace nx::vms::rules::utils
