#include "event_filter.h"

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

QString EventFilter::eventType() const
{
    return m_eventType;
}

 bool EventFilter::addField(const QString& name, EventField* field)
 {
    if (m_fields.contains(name))
        return false;

    m_fields[name] = field;
    return true;
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

} // namespace nx::vms::rules