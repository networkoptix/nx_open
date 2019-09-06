#include "event_filter.h"

#include <QScopedValueRollback>

#include "event_field.h"

namespace nx::vms::rules {

EventFilter::EventFilter(const QnUuid& id, const QString& eventType):
    m_id(id),
    m_eventType(eventType)
{
}

EventFilter::~EventFilter()
{
    qDeleteAll(m_fields);
}

QnUuid EventFilter::id() const
{
    return m_id;
}

QString EventFilter::eventType() const
{
    return m_eventType;
}

void EventFilter::addField(const QString& name, EventField* field)
{
    // TODO: assert?
    delete m_fields.value(name, nullptr);
    m_fields[name] = field;
    updateState();
}

const QHash<QString, EventField*>& EventFilter::fields() const
{
    return m_fields;
}

bool EventFilter::match(const EventPtr& event) const
{
    for (auto it = m_fields.begin(); it != m_fields.end(); ++it)
    {
        const QString& name = it.key();
        const EventField* field = it.value();

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
    for (auto& field: m_fields)
    {
        field->connectSignals();
        connect(field, &Field::stateChanged, this, &EventFilter::updateState);
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