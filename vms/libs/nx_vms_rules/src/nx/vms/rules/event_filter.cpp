// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_filter.h"

#include <vector>
#include <tuple>

#include <QtCore/QSharedPointer>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QVariant>

#include <nx/utils/log/log.h>

#include "basic_event.h"
#include "event_field.h"

namespace nx::vms::rules {

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

bool EventFilter::match(const EventPtr& event) const
{
    NX_VERBOSE(this, "Matching filter id: %1", m_id);

    for (const auto& [name, field]: m_fields)
    {
        const auto& value = event->property(name.toUtf8().data());
        NX_VERBOSE(this, "Matching property: %1, null: %2", name, value.isNull());

        if (value.isNull())
            return false;

        if (!field->match(value))
            return false;
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

} // namespace nx::vms::rules
