// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include "basic_event.h"

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::rules {

/** Wrapper around a list of event aggregation information. */
class NX_VMS_RULES_API AggregatedEvent: public QObject
{
    Q_OBJECT

public:
    explicit AggregatedEvent(const EventPtr& event);
    explicit AggregatedEvent(std::vector<EventPtr>&& eventList);
    ~AggregatedEvent() override = default;

    /** Returns property with the given name of the initial event. */
    QVariant property(const char* name) const;

    /** Returns initial event type. */
    QString type() const;

    /** Returns initial event timestamp. */
    std::chrono::microseconds timestamp() const;

    /** Returns initial event state. */
    State state() const;

    /** Event id, may be used for action deduplication.*/
    nx::Uuid id() const;

    /** Returns initial event details plus aggregated details. */
    QVariantMap details(common::SystemContext* context) const;

    using Filter = std::function<EventPtr(const EventPtr&)>;
    /**
     * Filters aggregated events by the given filter condition. If there is no appropriate events
     * nullptr returned.
     */
    AggregatedEventPtr filtered(const Filter& filter) const;

    using SplitKeyFunction = std::function<QString(const EventPtr&)>;
    /**
     * Split all the aggregated events to a list of AggregatedEventPtr using a key from the given
     * function. All the aggregated events are sorted by a timestamp.
     */
    std::vector<AggregatedEventPtr> split(const SplitKeyFunction& splitKeyFunction) const;

    size_t count() const;

    EventPtr initialEvent() const;

    const std::vector<EventPtr>& aggregatedEvents() const;

private:
    nx::Uuid m_id = nx::Uuid::createUuid(); //< TODO: #amalov Get from initial event if needed.
    std::vector<EventPtr> m_aggregatedEvents;

    AggregatedEvent() = default;
};

} // namespace nx::vms::rules
