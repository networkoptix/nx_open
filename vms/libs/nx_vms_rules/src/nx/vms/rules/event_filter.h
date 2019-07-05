#pragma once

#include "basic_event.h"

namespace nx::vms::rules {

class EventField;

class NX_VMS_RULES_API /*FieldBased*/EventFilter
{
public:
    EventFilter(const QnUuid& id, const QString& eventType);
    virtual ~EventFilter();

    QString eventType() const;

    bool addField(const QString& name, EventField* field);

    bool match(const EventPtr& event) const;

private:
    QnUuid m_id;
    QString m_eventType;
    QHash<QString, EventField*> m_fields;
};

} // namespace nx::vms::rules