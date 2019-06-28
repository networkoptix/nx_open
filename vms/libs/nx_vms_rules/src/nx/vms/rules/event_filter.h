#pragma once

#include "basic_event.h"

namespace nx::vms::rules {

class EventField;

class NX_VMS_RULES_API /*FieldBased*/EventFilter
{
public:
    EventFilter(const QnUuid& id);
    virtual ~EventFilter();

    bool match(const EventPtr& event) const;

private:
    QnUuid m_id;
    QHash<QString, EventField*> m_fields;
};

} // namespace nx::vms::rules