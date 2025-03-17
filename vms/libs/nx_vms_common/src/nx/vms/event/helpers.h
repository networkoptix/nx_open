// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <optional>

#include <QtCore/QStringList>

#include <core/resource/resource_fwd.h>
#include <nx/vms/event/event_fwd.h>

namespace nx::vms::event {

using EventTypePredicate = std::function<bool(EventType)>;
using EventTypePredicateList = QList<EventTypePredicate>;

/**
 * Predicate that returns true for an event that's not declared as deprecated.
 */
NX_VMS_COMMON_API bool isNonDeprecatedEvent(EventType eventType);

/**
 * @return Sorted list of event types for which each of the passed predicates returns true or list
 *     of all known event types if no predicate passed as a parameter.
 */
NX_VMS_COMMON_API QList<EventType> allEvents(
    const EventTypePredicateList& predicates = {isNonDeprecatedEvent});

/** Check if resource required to SETUP rule on this event. */
NX_VMS_COMMON_API bool isResourceRequired(EventType eventType);

/** Check if camera required for this event to setup a rule. */
NX_VMS_COMMON_API bool requiresCameraResource(EventType eventType);

/** Check if server required for this event to setup a rule. */
NX_VMS_COMMON_API bool requiresServerResource(EventType eventType);

/**
 * Finds all resources related to this event.
 * - std::nullopt - means there are no any resources.
 * - QnResourceList - the list of resources, that could be found. Empty list means there were some
 * incorrect id's in the list.
 */
NX_VMS_COMMON_API std::optional<QnResourceList> sourceResources(
    const EventParameters& params,
    const QnResourcePool* resourcePool,
    const std::function<void(const QString&)>& notFound = nullptr);

NX_VMS_COMMON_API QStringList splitOnPureKeywords(const QString& keywords);
NX_VMS_COMMON_API bool checkForKeywords(const QString& value, const QStringList& keywords);

NX_VMS_COMMON_API QList<ActionType> userAvailableActions();
NX_VMS_COMMON_API QList<ActionType> allActions();

NX_VMS_COMMON_API bool requiresUserResource(ActionType actionType);
NX_VMS_COMMON_API bool isActionProlonged(ActionType actionType, const ActionParameters &parameters);


} // namespace nx::vms::event
