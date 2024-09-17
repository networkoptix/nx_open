// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <core/resource/resource_fwd.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/event_parameters.h>

namespace nx::vms::common { class SystemContext; }

namespace nx {
namespace vms {
namespace event {

NX_VMS_COMMON_API bool hasChild(EventType eventType);

NX_VMS_COMMON_API QList<EventType> childEvents(EventType eventType);

using EventTypePredicate = std::function<bool(EventType)>;
using EventTypePredicateList = QList<EventTypePredicate>;

/**
 * Predicate that returns true for an event that's not declared as deprecated.
 */
NX_VMS_COMMON_API bool isNonDeprecatedEvent(EventType eventType);

/**
 * @return Predicate that returns true for an event that is supported by the licensing model of
 *     the system described by the given system context.
 */
NX_VMS_COMMON_API EventTypePredicate isApplicableForLicensingMode(
    nx::vms::common::SystemContext* systemContext);

/**
 * @return Predicate that returns true for an event type if it's child of the given event type.
 */
NX_VMS_COMMON_API EventTypePredicate isChildOf(EventType parentEventType);

/**
 * @return Sorted list of event types for which each of the passed predicates returns true or list
 *     of all known event types if no predicate passed as a parameter.
 */
NX_VMS_COMMON_API QList<EventType> allEvents(
    const EventTypePredicateList& predicates = {isNonDeprecatedEvent});

NX_VMS_COMMON_API EventType parentEvent(EventType eventType);

/** Check if resource required to SETUP rule on this event. */
NX_VMS_COMMON_API bool isResourceRequired(EventType eventType);

/**
 * By having the 'toggle state' the old engine means the event may be prolonged,
 * i.e. have start and stop states.
 */
NX_VMS_COMMON_API bool hasToggleState(
    EventType eventType,
    const EventParameters& runtimeParams,
    nx::vms::common::SystemContext* systemContext);

NX_VMS_COMMON_API QList<EventState> allowedEventStates(
    EventType eventType,
    const EventParameters& runtimeParams,
    nx::vms::common::SystemContext* systemContext);

/** Check if camera required for this event to setup a rule. */
NX_VMS_COMMON_API bool requiresCameraResource(EventType eventType);

/** Check if server required for this event to setup a rule. */
NX_VMS_COMMON_API bool requiresServerResource(EventType eventType);

/** Check if camera required for this event to OCCUR. Always includes requiresCameraResource(). */
NX_VMS_COMMON_API bool isSourceCameraRequired(EventType eventType);

/** Check if server required for this event to OCCUR. Always includes requiresServerResource(). */
NX_VMS_COMMON_API bool isSourceServerRequired(EventType eventType);

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

/** Checks if the user has an access to this event. */
NX_VMS_COMMON_API bool hasAccessToSource(const EventParameters& params, const QnUserResourcePtr& user);

/**
 * @brief The AbstractEvent class
 *                              Base class for business events. Contains parameters of the
 *                              occurred event and methods for checking it against the rules.
 *                              No classes should directly inherit AbstractEvent
 *                              except the QnInstantBusinessEvent and QnProlongedBusinessEvent.
 */
class NX_VMS_COMMON_API AbstractEvent
{
protected:
    /**
     * @brief AbstractEvent
     *                          Explicit constructor that MUST be overridden in descendants.
     * @param eventType         Type of the event.
     * @param resource          Resources that provided the event.
     * @param toggleState       On/off state of the event if it is toggleable.
     * @param timeStamp         Event date and time in usec from UTC.
     */
    AbstractEvent(EventType eventType, const QnResourcePtr& resource, EventState toggleState, qint64 timeStamp);

public:
    virtual ~AbstractEvent();

    /**
     * @brief getResource       Get resource that provided this event.
     * @return                  Shared pointer on the resource.
     */
    const QnResourcePtr& getResource() const { return m_resource; }

    /**
     * @brief getEventType      Get type of event. See EventType.
     * @return                  Enumeration value on the event.
     */
    EventType getEventType() const { return m_eventType; }

    /**
     * @return                  On/off state of the event.
     */
    EventState getToggleState()     const { return m_toggleState; }

    /** Event timestamp in microseconds elapsed since start of the epoch. */
    std::chrono::microseconds getTimestamp() const { return m_timeStamp; };

    /**
     * @brief isEventStateMatched Checks if event state matches to a rule. Rule will be terminated if it isn't pass any longer
     * @return                    True if event should be handled, false otherwise.
     */
    virtual bool isEventStateMatched(EventState state, ActionType actionType) const = 0;

    /**
     * @brief checkCondition      Checks if event params matches to a rule. Rule will not start if it isn't pass
     * @param params              Parameters of an event that are selected in rule.
     * @return                    True if event should be handled, false otherwise.
     */
    virtual bool checkEventParams(const EventParameters &params) const;

    /**
    * @brief getRuntimeParams     Fills event parameters structure with runtime event information
    */
    virtual EventParameters getRuntimeParams() const;

    /**
    * @brief getRuntimeParams     Fills event parameters structure with runtime event information
    *                             and additional information from business rule event parameters
    */
    virtual EventParameters getRuntimeParamsEx(
        const EventParameters& ruleEventParams) const;

    /**
     * @return external event key. It is used in case of several prolonged events are running on
     * the same event rule. It is needed for correct calculation of event start/stop signals.
     * It could be a camera input port id, analytics object type e.t.c.
     */
    virtual QString getExternalUniqueKey() const;
private:
    /**
     * @brief m_eventType       Type of event. See EventType.
     */
    const EventType m_eventType;

    /**
     * @brief m_timeStamp       Event date and time in usec from UTC.
     */
    const std::chrono::microseconds m_timeStamp;

    /**
     * @brief m_resource        Resource that provide this event.
     */
    const QnResourcePtr m_resource;

    /**
     * @brief m_toggleState     State on/off for toggleable events.
     */
    const EventState m_toggleState;
};

} // namespace event
} // namespace vms
} // namespace nx
