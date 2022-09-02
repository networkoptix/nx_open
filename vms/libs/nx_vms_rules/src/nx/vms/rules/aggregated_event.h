// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include "aggregator.h"
#include "basic_event.h"

namespace nx::vms::rules {

/** Wrapper around a list of event aggregation information. */
class NX_VMS_RULES_API AggregatedEvent: public QObject
{
    Q_OBJECT

public:
    using Filter = std::function<EventPtr(const EventPtr&)>;

public:
    explicit AggregatedEvent(const EventPtr& event);
    explicit AggregatedEvent(const AggregationInfo& aggregationInfo);
    explicit AggregatedEvent(const AggregationInfoList& aggregationInfoList);
    ~AggregatedEvent() = default;

    /** Returns property with the given name of the initial event. */
    QVariant property(const char* name) const;

    /** Returns initial event type. */
    QString type() const;

    /** Returns initial event timestamp. */
    std::chrono::microseconds timestamp() const;

    /** Event id, may be used for action deduplication.*/
    QnUuid id() const;

    /** Returns initial event details plus aggregated details. */
    QVariantMap details(common::SystemContext* context) const;

    /**
     * Filters aggregated events by the given filter condition. If there is no appropriate events
     * nullptr returned.
     */
    AggregatedEventPtr filtered(const Filter& filter) const;

    size_t totalCount() const;
    size_t uniqueCount() const;

private:
    QnUuid m_id = QnUuid::createUuid(); //< TODO: #amalov Get from initial event if needed.
    AggregationInfoList m_aggregationInfoList;

    AggregatedEvent() = default;
    EventPtr initialEvent() const;
};

} // namespace nx::vms::rules
