// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_filter.h"

#include <tuple>
#include <vector>

#include <QtCore/QJsonValue>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QSharedPointer>
#include <QtCore/QVariant>

#include <nx/utils/log/log.h>
#include <utils/common/synctime.h>

#include "basic_event.h"
#include "event_field.h"
#include "event_fields/state_field.h"
#include "utils/field.h"
#include "utils/type.h"

namespace nx::vms::rules {

namespace {

using namespace std::chrono;

// The following timeouts got from the event rule processor.
// Timeout shouldn't be too long because new events are filtered out during this timeout and actions
// could be triggered too rare.
constexpr auto kEventTimeout = 3s;
constexpr auto kCleanupTimeout = 5s;

bool isProlonged(const EventPtr& event)
{
    const auto eventState = event->state();
    return eventState == State::started || eventState == State::stopped;
}

} // namespace

EventFilter::EventFilter(const QnUuid& id, const QString& eventType):
    m_id(id),
    m_eventType(eventType)
{
    NX_ASSERT(!id.isNull());
    NX_ASSERT(!m_eventType.isEmpty());
}

EventFilter::~EventFilter()
{
}

QnUuid EventFilter::id() const
{
    return m_id;
}

QString EventFilter::eventType() const
{
    return m_eventType;
}

std::map<QString, QVariant> EventFilter::flatData() const
{
    std::map<QString, QVariant> result;
    for (const auto& [id, field]: m_fields)
    {
        const auto& fieldProperties = field->serializedProperties();
        for (auto it = fieldProperties.begin(); it != fieldProperties.end(); ++it)
        {
            const QString path = id + "/" + it.key();
            result[path] = it.value();
        }
    }
    return result;
}

bool EventFilter::updateData(const QString& path, const QVariant& value)
{
    const auto ids = path.split('/');
    if (ids.size() != 2)
        return false;
    if (m_fields.find(ids[0]) == m_fields.end())
        return false;

    m_fields[ids[0]]->setProperty(ids[1].toLatin1().data(), value);
    return true;
}

bool EventFilter::updateFlatData(const std::map<QString, QVariant>& data)
{
    std::vector<std::tuple<QString, QString, QVariant>> parsed;

    /* Check data. */
    for (const auto& [id, value]: data)
    {
        const auto ids = id.split('/');
        if (ids.size() != 2)
            return false;
        if (m_fields.find(ids[0]) == m_fields.end())
         return false;
        parsed.push_back({ids[0], ids[1], value});
    }

    /* Update. */
    for (const auto& [field, prop, value]: parsed)
    {
        m_fields[field]->setProperty(prop.toLatin1().data(), value);
    }
    return true;
}

void EventFilter::addField(const QString& name, std::unique_ptr<EventField> field)
{
    // TODO: assert?
    m_fields[name] = std::move(field);
    updateState();
}

const QHash<QString, EventField*> EventFilter::fields() const
{
    QHash<QString, EventField*> result;
    for (const auto& [name, field]: m_fields)
    {
        result[name] = field.get();
    }
    return result;
}

void EventFilter::cleanupOldEventsFromCache(
    std::chrono::milliseconds eventTimeout,
    std::chrono::milliseconds cleanupTimeout) const
{
    if (!m_cacheTimer.hasExpired(cleanupTimeout))
        return;
    m_cacheTimer.restart();

    // TODO: can be changed to the std::remove_if after
    // C++20 upgrade: https://en.cppreference.com/w/cpp/container/map/erase_if
    auto itr = m_cachedEvents.begin();
    while (itr != m_cachedEvents.end())
    {
        if (itr->second.hasExpired(eventTimeout))
            itr = m_cachedEvents.erase(itr);
        else
            ++itr;
    }
}

bool EventFilter::wasEventCached(
    std::chrono::milliseconds eventTimeout,
    const QString& cacheKey) const
{
    const auto it = m_cachedEvents.find(cacheKey);
    return it != m_cachedEvents.end()
        && !it->second.hasExpired(eventTimeout);
}

void EventFilter::cacheEvent(const QString& eventKey) const
{
    using namespace nx::utils;

    if (eventKey.isEmpty())
        return;

    cleanupOldEventsFromCache(kEventTimeout, kCleanupTimeout);
    m_cachedEvents.emplace(eventKey, ElapsedTimer(ElapsedTimerState::started));
}

bool EventFilter::match(const EventPtr& event) const
{
    NX_VERBOSE(this, "Matching filter id: %1", m_id);

    const auto cacheKey = event->cacheKey();
    if (wasEventCached(kEventTimeout, cacheKey))
    {
        NX_VERBOSE(this, "Skipping cached event with key: %1", cacheKey);
        return false;
    }

    if (!matchFields(event))
        return false;

    // Cache and limit repeat of event with overloaded cacheKey().
    cacheEvent(cacheKey);

    // TODO: #mmalofeev Consider move this check to the engine.
    if (isProlonged(event))
    {
        // It is required to check if the prolonged events are coming in the right order - for the
        // each stopped event musts preceded started event. Also the same event must not be
        // processed twice with the same state.

        const auto resourceKey = event->resourceKey();
        const bool isEventRunning = m_runningEvents.contains(resourceKey);

        if (event->state() == State::started)
        {
            if (isEventRunning)
            {
                NX_VERBOSE(
                    this,
                    "Event %1-%2 doesn't match since started state already handled",
                    event->type(),
                    resourceKey);
                return false;
            }

            m_runningEvents.insert(resourceKey);
        }
        else
        {
            if (!isEventRunning)
            {
                NX_VERBOSE(
                    this,
                    "Event %1-%2 doesn't match since before the stopped, started state must be received",
                    event->type(),
                    resourceKey);
                return false;
            }

            m_runningEvents.erase(resourceKey);
        }
    }

    if (!matchState(event))
        return false;

    NX_VERBOSE(this, "Matched filter id: %1", m_id);
    return true;
}

// Match all event fields excluding state.
bool EventFilter::matchFields(const EventPtr& event) const
{
    for (const auto& [name, field]: m_fields)
    {
        if (name == utils::kStateFieldName)
            continue;

        const auto& value = event->property(name.toUtf8().data());
        NX_VERBOSE(this, "Matching property: %1, valid: %2, null: %3",
            name, value.isValid(), value.isNull());

        if (!value.isValid())
            return false;

        if (!field->match(value))
            return false;
    }

    return true;
}

// Match state field only. Consider storing it as a separate member.
bool EventFilter::matchState(const EventPtr& event) const
{
    NX_VERBOSE(this, "Matching event state: %1", event->state());

    if (const auto stateField = fieldByName<StateField>(utils::kStateFieldName))
        return stateField->match(QVariant::fromValue(event->state()));

    return true;
}

void EventFilter::connectSignals()
{
    for (auto& [id, field]: m_fields)
    {
        field->connectSignals();
        connect(field.get(), &Field::stateChanged, this, &EventFilter::updateState);
    }
}

void EventFilter::updateState()
{
    //TODO: #spanasenko Update derived values (error messages, etc.)

    if (m_updateInProgress)
        return;

    QScopedValueRollback<bool> guard(m_updateInProgress, true);
    emit stateChanged();
}

} // namespace nx::vms::rules
