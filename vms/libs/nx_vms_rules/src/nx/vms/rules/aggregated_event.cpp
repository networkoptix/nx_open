// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "aggregated_event.h"

#include <unordered_map>

#include <QtCore/QDateTime>
#include <QtCore/QVariant>

#include <nx/utils/log/assert.h>
#include <nx/utils/qobject.h>
#include <nx/vms/api/rules/event_log.h>
#include <nx/vms/time/formatter.h>

#include "engine.h"
#include "strings.h"
#include "utils/serialization.h"

namespace nx::vms::rules {

using namespace nx::vms::api::rules;

AggregatedEvent::AggregatedEvent(const EventPtr& event):
    m_aggregatedEvents{event},
    m_count(1)
{
}

AggregatedEvent::AggregatedEvent(std::vector<EventPtr>&& eventList):
    m_aggregatedEvents{std::move(eventList)},
    m_count(m_aggregatedEvents.size())
{
}

AggregatedEvent::AggregatedEvent(Engine* engine, const EventLogRecord& record):
    m_count(std::max(1, record.aggregatedInfo.total)) //< Event count can't be less than one.
{
    m_aggregatedEvents.push_back(engine->buildEvent(record.eventData));
    for (const auto& record: record.aggregatedInfo.firstEventsData)
        m_aggregatedEvents.push_back(engine->buildEvent(record));
    // Probably we should add a spacer here.
    for (const auto& record: record.aggregatedInfo.lastEventsData)
        m_aggregatedEvents.push_back(engine->buildEvent(record));
}

std::pair<std::vector<EventPtr> /*firstEvents*/, std::vector<EventPtr> /*lastEvents*/>
    AggregatedEvent::takeLimitedAmount(int limit) const
{
    // Unit tests environment.
    if (m_eventLimitOverload > 0)
    {
        NX_ASSERT(limit == AggregatedEvent::kTotalEventsLimit,
            "Passing limit directly does not work with an overload");
        limit = m_eventLimitOverload;
    }

    // Check real events count here to ensure events would be correctly split if needed.
    if ((int) count() <= limit)
        return {m_aggregatedEvents, {}};

    // If the event is build from log record data, the real m_aggregatedEvents amount can be less
    // than the limit.
    limit = std::min(limit, (int) m_aggregatedEvents.size());

    const int first = (limit + 1) / 2; //< Ensure odd values will be rounded up.
    const int last = limit - first;

    return {
        std::vector<EventPtr>(m_aggregatedEvents.cbegin(), m_aggregatedEvents.cbegin() + first),
        std::vector<EventPtr>(m_aggregatedEvents.cend() - last, m_aggregatedEvents.cend()),
    };
}

void AggregatedEvent::overloadDefaultEventLimit(int value)
{
    m_eventLimitOverload = value;
}

QString AggregatedEvent::type() const
{
    return m_aggregatedEvents.empty() ? QString{} : initialEvent()->type();
}

QVariant AggregatedEvent::property(const char* name) const
{
    return m_aggregatedEvents.empty() ? QVariant{} : initialEvent()->property(name);
}

std::chrono::microseconds AggregatedEvent::timestamp() const
{
    return m_aggregatedEvents.empty()
        ? std::chrono::microseconds::zero()
        : initialEvent()->timestamp();
}

State AggregatedEvent::state() const
{
    return NX_ASSERT(!m_aggregatedEvents.empty()) ? initialEvent()->state() : State::none;
}

const QVariantMap& AggregatedEvent::details(
    common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    static QVariantMap kEmptyValue;
    if (m_aggregatedEvents.empty())
        return kEmptyValue;

    auto cachedValue = m_detailsCache.find(detailLevel);
    if (cachedValue != m_detailsCache.cend())
        return cachedValue->second;

    m_detailsCache[detailLevel] = initialEvent()->details(context, detailLevel);
    return m_detailsCache[detailLevel];
}

AggregatedEventPtr AggregatedEvent::filtered(const Filter& filter) const
{
    NX_ASSERT(m_aggregatedEvents.size() == m_count,
        "This function is not allowed for deserialized events as they contain limited info.");

    if (m_aggregatedEvents.empty())
        return {};

    std::vector<EventPtr> filteredList;
    for (const auto& aggregatedEvent: m_aggregatedEvents)
    {
        if (auto event = filter(aggregatedEvent))
            filteredList.emplace_back(std::move(event));
    }

    if (filteredList.empty())
        return {};

    return AggregatedEventPtr::create(std::move(filteredList));
}

std::vector<AggregatedEventPtr> AggregatedEvent::split(
    const SplitKeyFunction& splitKeyFunction) const
{
    NX_ASSERT(m_aggregatedEvents.size() == m_count,
        "This function is not allowed for deserialized events as they contain limited info.");

    if (m_aggregatedEvents.empty())
        return {};

    std::unordered_map<QString, std::vector<EventPtr>> splitMap;
    for (const auto& aggregatedEvent: m_aggregatedEvents)
        splitMap[splitKeyFunction(aggregatedEvent)].push_back(aggregatedEvent);

    std::vector<AggregatedEventPtr> result;
    result.reserve(splitMap.size());
    for (auto& [_, list]: splitMap)
        result.push_back(AggregatedEventPtr::create(std::move(list)));

    std::sort(
        result.begin(),
        result.end(),
        [](const AggregatedEventPtr& l, const AggregatedEventPtr& r)
        {
            return l->timestamp() < r->timestamp();
        });

    return result;
}

size_t AggregatedEvent::count() const
{
    return m_count;
}

EventPtr AggregatedEvent::initialEvent() const
{
    return m_aggregatedEvents.empty() ? EventPtr{} : m_aggregatedEvents.front();
}

nx::Uuid AggregatedEvent::ruleId() const
{
    return m_ruleId;
}

void AggregatedEvent::setRuleId(nx::Uuid ruleId)
{
    m_ruleId = ruleId;
}

void AggregatedEvent::storeToRecord(EventLogRecord* record, int limit) const
{
    if (!NX_ASSERT(record))
        return;

    AggregatedInfo& aggregatedInfo = record->aggregatedInfo;

    auto serializeEvent =
        [](const EventPtr& event)
        {
            return serializeProperties(
                event.get(),
                nx::utils::propertyNames(
                    event.get(),
                    nx::utils::PropertyAccess::anyAccess | nx::utils::PropertyAccess::stored));
        };

    auto [firstEvents, lastEvents] = takeLimitedAmount(limit);

    if (NX_ASSERT(firstEvents.size() > 0))
    {
        record->eventData = serializeEvent(firstEvents.front());
        firstEvents.erase(firstEvents.begin());
    }

    for (const auto& event: firstEvents)
        aggregatedInfo.firstEventsData.push_back(serializeEvent(event));
    for (const auto& event: lastEvents)
        aggregatedInfo.lastEventsData.push_back(serializeEvent(event));

    aggregatedInfo.total = m_aggregatedEvents.size();
}

} // namespace nx::vms::rules
