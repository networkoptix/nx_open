// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

#include "../manifest.h"
#include "../rules_fwd.h"

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

/** Returns whether any of the given servers support given event or action. */
NX_VMS_RULES_API bool hasItemSupportedServer(
    const QnMediaServerResourceList& servers, const ItemDescriptor& itemDescriptor);

/**
 * Returns whether given event and action are compatible.
 * Instant event is compatible with instant action and with the prolonged action with fixed duration.
 * Prolonged event is compatible with the prolonged and instant actions.
 */
NX_VMS_RULES_API bool isCompatible(
    const ItemDescriptor& eventDescriptor, const ItemDescriptor& actionDescriptor);

/**
 * Returns whether value in the given state field is compatible with the given event filter.
 * `State::instant` is compatible with the instant event, all the rest states are compatible with
 * the prologed events.
 */
NX_VMS_RULES_API bool isCompatible(
    const Engine* engine, const StateField* stateField, const EventFilter* eventFilter);

/**
 * Returns whether value in the given state field is compatible with the given action builder.
 * Prolonged action compatible only with `State::none` value. Actions with fixed duration or instant
 * actions compatible with all state values, except `State::none`.
 */
NX_VMS_RULES_API bool isCompatible(
    const Engine* engine, const StateField* stateField, const ActionBuilder* actionBuilder);

/**
 * Return whether the given event filter is compatible with the given action builder.
 * The function checks whether selected event and action types are compatible and whether event
 * state in the event filter is compatible for the given event type and for the given action builder.
 */
NX_VMS_RULES_API bool isCompatible(
    const Engine* engine, const EventFilter* eventFilter, const ActionBuilder* actionBuilder);

} // namespace nx::vms::rules::utils
