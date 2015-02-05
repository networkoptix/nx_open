#include "conflict_business_event.h"

QnConflictBusinessEvent::QnConflictBusinessEvent(const QnBusiness::EventType eventType,
                                                 const QnResourcePtr& resource,
                                                 const qint64 timeStamp,
                                                 const QString& source,
                                                 const QStringList& conflicts):
    base_type(eventType, resource, timeStamp),
    m_source(source),
    m_conflicts(conflicts)
{
}

QnBusinessEventParameters QnConflictBusinessEvent::getRuntimeParams() const {
    QnBusinessEventParameters params = base_type::getRuntimeParams();
    params.source = m_source;
    params.conflicts = m_conflicts;
    return params;
}
