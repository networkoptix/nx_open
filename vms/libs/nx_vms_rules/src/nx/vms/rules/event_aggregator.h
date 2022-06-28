// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>

#include "basic_event.h"

namespace nx::vms::rules {

/** Aggregates events by their unique name. */
class NX_VMS_RULES_API EventAggregator: public QObject
{
    Q_OBJECT

public:
    explicit EventAggregator(const EventPtr& event);
    ~EventAggregator() = default;

    void aggregate(const EventPtr& event);

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
    EventAggregatorPtr filtered(const std::function<bool(const EventPtr&)>& filter) const;

    size_t totalCount() const;
    size_t uniqueCount() const;

private:
    EventPtr m_initialEvent;
    QHash</*unique name*/ QString, QPair<EventPtr, /*count*/ size_t>> m_aggregatedEvents;

    EventAggregator() = default;
};

} // namespace nx::vms::rules
