// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>

#include "aggregator.h"
#include "basic_event.h"

namespace nx::vms::rules {

/** Wrapper around a list of event aggregation information. */
class NX_VMS_RULES_API AggregatedEvent: public QObject
{
    Q_OBJECT

public:
    explicit AggregatedEvent(const EventPtr& event);
    explicit AggregatedEvent(const AggregationInfo& aggregationInfo);
    explicit AggregatedEvent(const std::list<AggregationInfo>& aggregationInfoList);
    ~AggregatedEvent() = default;

    /** Returns property with the given name of the initial event. */
    QVariant property(const char* name) const;

    /** Returns initial event type. */
    QString type() const;

    /** Returns initial event timestamp. */
    std::chrono::microseconds timestamp() const;

    /** Returns initial event details plus aggregated details. */
    QVariantMap details(common::SystemContext* context) const;

    /**
     * Filters aggregated events by the given filter condition. If there is no appropriate events
     * nullptr returned.
     */
    AggregatedEventPtr filtered(const std::function<bool(const EventPtr&)>& filter) const;

    size_t totalCount() const;
    size_t uniqueCount() const;

private:
    std::list<AggregationInfo> m_aggregationInfoList;

    AggregatedEvent() = default;
    EventPtr initialEvent() const;
};

} // namespace nx::vms::rules
