// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <nx/vms/api/rules/aggregated_info.h>

#include "basic_event.h"

namespace nx::vms::common { class SystemContext; }
namespace nx::vms::api::rules { struct EventLogRecord; }

namespace nx::vms::rules {

/**
 * Wrapper around a list of event aggregation information. May be constructed directly from the
 * list of events - or from a serialized aggregation information.
 */
class NX_VMS_RULES_API AggregatedEvent: public QObject
{
    Q_OBJECT
    using AggregatedInfo = nx::vms::api::rules::AggregatedInfo;
    using EventLogRecord = nx::vms::api::rules::EventLogRecord;

public:
    explicit AggregatedEvent(const EventPtr& event);
    explicit AggregatedEvent(std::vector<EventPtr>&& eventList);

    /** Constructor from the serialized data. */
    AggregatedEvent(Engine* engine, const EventLogRecord& record);

    ~AggregatedEvent() override = default;

    static constexpr int kTotalEventsLimit = 10;

    /**
     * Take limited number of events from this aggregated instance.
     *
     * Take only `limit` of entries from the event list: half of the number from the beginning of
     * the list, and the rest from the end of the list. Thus user will see when events started and
     * when they ended.
     */
    std::pair<std::vector<EventPtr> /*firstEvents*/, std::vector<EventPtr> /*lastEvents*/>
        takeLimitedAmount(int limit = kTotalEventsLimit) const;

    /**
     * Set new default limit of events used in the takeLimitedAmount function. Intended for usage
     * in unit tests only.
     */
    void overloadDefaultEventLimit(int value);

    /** Returns property with the given name of the initial event. */
    QVariant property(const char* name) const;

    /** Returns initial event type. */
    QString type() const;

    /** Returns initial event timestamp. */
    std::chrono::microseconds timestamp() const;

    /** Returns initial event state. */
    State state() const;

    /** Returns initial event details plus aggregated details. */
    const QVariantMap& details(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const;

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

    /**
     * Serialize data to the Event Log record format. Only a few fields are stored.
     * @param record Record to store event data and aggregation data.
     */
    void storeToRecord(
        EventLogRecord* record,
        int limit = kTotalEventsLimit) const;

    nx::Uuid ruleId() const;
    void setRuleId(nx::Uuid ruleId);

private:
    std::vector<EventPtr> m_aggregatedEvents;

    /**
    * When the event is constructed from the log record, m_aggregatedEvents may contain much less
    * events than there were in the original aggregated event. Keep the total count separately.
    */
    const size_t m_count;

    /** When provided, value passed to takeLimitedAmount function will be ignored. */
    int m_eventLimitOverload = 0;

    nx::Uuid m_ruleId;

    /** Cache of the details method call. */
    mutable std::map<Qn::ResourceInfoLevel, QVariantMap> m_detailsCache;
};

} // namespace nx::vms::rules
