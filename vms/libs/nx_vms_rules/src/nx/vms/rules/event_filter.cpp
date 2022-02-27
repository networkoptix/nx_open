// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_filter.h"

#include <QtCore/QSharedPointer>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QVariant>

#include "event_field.h"

namespace nx::vms::rules {

EventFilter::EventFilter(const QnUuid& id, const QString& eventType):
    m_id(id),
    m_eventType(eventType)
{
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
    for (const auto& [name, field]: m_fields)
    {
        const auto& value = event->property(name.toUtf8().data());
        if (value.isNull())
            return false;

        if (!field->match(value))
            return false;
    }
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
