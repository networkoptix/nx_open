#include "event_filter.h"

#include "event_field.h"

namespace nx::vms::rules {

EventFilter::EventFilter(const QnUuid& id): m_id(id)
{
}

EventFilter::~EventFilter()
{
    qDeleteAll(m_fields);
}

bool EventFilter::match(const EventPtr& event) const
{
    for (auto it = m_fields.begin(); it != m_fields.end(); ++it)
    {
        const QString& name = it.key();
        const EventField* field = it.value();

        const auto& value = event->property(name.toLatin1().data());
        if (value.isNull())
            return false;

        if (!field->match(value))
            return false;
    }
    return true;
}

} // namespace nx::vms::rules