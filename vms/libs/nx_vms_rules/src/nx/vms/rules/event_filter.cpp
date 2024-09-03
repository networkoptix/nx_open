// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_filter.h"

#include <tuple>
#include <vector>

#include <QtCore/QJsonValue>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QSharedPointer>
#include <QtCore/QVariant>

#include <nx/utils/log/log.h>
#include <nx/utils/qobject.h>
#include <utils/common/synctime.h>

#include "basic_event.h"
#include "engine.h"
#include "event_cache.h"
#include "event_filter_field.h"
#include "event_filter_fields/state_field.h"
#include "rule.h"
#include "utils/event.h"
#include "utils/field.h"
#include "utils/type.h"

namespace nx::vms::rules {

EventFilter::EventFilter(nx::Uuid id, const QString& eventType):
    m_id(id),
    m_eventType(eventType)
{
    NX_ASSERT(!m_eventType.isEmpty());
    NX_ASSERT(!m_id.isNull());
}

EventFilter::~EventFilter()
{
}

nx::Uuid EventFilter::id() const
{
    return m_id;
}

QString EventFilter::eventType() const
{
    return m_eventType;
}

const Rule* EventFilter::rule() const
{
    return m_rule;
}

void EventFilter::setRule(const Rule* rule)
{
    m_rule = rule;
}

void EventFilter::addField(const QString& name, std::unique_ptr<EventFilterField> field)
{
    field->moveToThread(thread());

    static const auto onFieldChangedMetaMethod =
        metaObject()->method(metaObject()->indexOfMethod("onFieldChanged()"));
    if (NX_ASSERT(onFieldChangedMetaMethod.isValid()))
        nx::utils::watchOnPropertyChanges(field.get(), this, onFieldChangedMetaMethod);

    m_fields[name] = std::move(field);
}

QHash<QString, EventFilterField*> EventFilter::fields() const
{
    QHash<QString, EventFilterField*> result;
    for (const auto& [name, field]: m_fields)
    {
        result[name] = field.get();
    }
    return result;
}

bool EventFilter::match(const EventPtr& event) const
{
    NX_VERBOSE(this, "Matching filter id: %1", m_id);

    if (!matchState(event) || !matchFields(event))
        return false;

    NX_VERBOSE(this, "Matched filter id: %1", m_id);
    return true;
}

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


bool EventFilter::matchState(const EventPtr& event) const
{
    NX_VERBOSE(this, "Matching event state: %1", event->state());

    if (const auto stateField = fieldByName<StateField>(utils::kStateFieldName))
        return stateField->match(QVariant::fromValue(event->state()));

    return true;
}

void EventFilter::onFieldChanged()
{
    const auto changedField = sender();

    for (const auto& [fieldName, field]: m_fields)
    {
        if (field.get() == changedField)
        {
            emit changed(fieldName);
            return;
        }
    }

    NX_ASSERT(false, "Must not be here");
}

} // namespace nx::vms::rules
