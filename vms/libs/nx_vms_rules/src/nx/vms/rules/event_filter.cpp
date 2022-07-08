// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_filter.h"

#include <tuple>
#include <vector>

#include <QtCore/QScopedValueRollback>
#include <QtCore/QSharedPointer>
#include <QtCore/QVariant>

#include <nx/utils/log/log.h>
#include <utils/common/synctime.h>

#include "basic_event.h"
#include "event_field.h"
#include "events/analytics_object_event.h"
#include "utils/type.h"

namespace nx::vms::rules {

namespace {

// The following timeouts got from the event rule processor.
constexpr std::chrono::seconds kObjectDetectedEventTimeout(30);
constexpr std::chrono::seconds kCleanupTimeout(5);

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

    connect(&m_clearCacheTimer, &QTimer::timeout, this, &EventFilter::clearCache);
    m_clearCacheTimer.setInterval(kCleanupTimeout);
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

bool EventFilter::match(const EventPtr& event) const
{
    NX_VERBOSE(this, "Matching filter id: %1", m_id);

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

    for (const auto& [name, field]: m_fields)
    {
        const auto& value = event->property(name.toUtf8().data());
        NX_VERBOSE(this, "Matching property: %1, null: %2", name, value.isNull());

        if (value.isNull())
            return false;

        if (!field->match(value))
            return false;
    }

    // At the moment only AnalyticsObjectEvent might be suppressed and processed not often than once
    // per kObjectDetectedEventTimeout interval.
    const bool isSuppressibleEvent = event->type() == utils::type<AnalyticsObjectEvent>();
    // TODO: #mmalofeev Consider move this check to the engine.
    if (isSuppressibleEvent)
    {
        const auto currentTimePoint = qnSyncTime->currentTimePoint();
        const auto resourceKey = event->resourceKey();
        if (auto it = m_suppressedEvents.find(event->uniqueName()); it != m_suppressedEvents.end())
        {
            if (currentTimePoint - it->second < kObjectDetectedEventTimeout)
            {
                NX_VERBOSE(
                    this,
                    "Event %1-%2 doesn't match since mightn't be processed often than once per %3 sec",
                    event->type(),
                    resourceKey,
                    kObjectDetectedEventTimeout.count());
                return false;
            }
            else
            {
                m_suppressedEvents.erase(it);
            }
        }
        else
        {
            m_suppressedEvents.insert({resourceKey, currentTimePoint});

            if (!m_clearCacheTimer.isActive())
                m_clearCacheTimer.start();
        }
    }

    NX_VERBOSE(this, "Matched filter id: %1", m_id);
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

void EventFilter::clearCache()
{
    const auto now = qnSyncTime->currentTimePoint();
    for (auto it = m_suppressedEvents.begin(); it != m_suppressedEvents.end();)
    {
        if (now - it->second >= kObjectDetectedEventTimeout)
            it = m_suppressedEvents.erase(it);
        else
            ++it;
    }

    if (m_suppressedEvents.empty())
        m_clearCacheTimer.stop();
}

} // namespace nx::vms::rules
