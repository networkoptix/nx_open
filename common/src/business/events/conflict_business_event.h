#ifndef CONFLICT_BUSINESS_EVENT_H
#define CONFLICT_BUSINESS_EVENT_H

#include "instant_business_event.h"
#include "core/resource/resource_fwd.h"

class QnConflictBusinessEvent : public QnInstantBusinessEvent
{
    typedef QnInstantBusinessEvent base_type;
public:
    explicit QnConflictBusinessEvent(const BusinessEventType::Value eventType,
                                     const QnResourcePtr& resource,
                                     const qint64 timeStamp,
                                     const QByteArray& source = QByteArray(),
                                     const QList<QByteArray>& conflicts = QList<QByteArray>());
    
    virtual QnBusinessEventParameters getRuntimeParams() const override;
protected:
    QByteArray m_source;
    QList<QByteArray> m_conflicts;
};

#endif // CONFLICT_BUSINESS_EVENT_H
