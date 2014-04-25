#ifndef CONFLICT_BUSINESS_EVENT_H
#define CONFLICT_BUSINESS_EVENT_H

#include "instant_business_event.h"
#include "core/resource/resource_fwd.h"

class QnConflictBusinessEvent : public QnInstantBusinessEvent
{
    typedef QnInstantBusinessEvent base_type;
public:
    explicit QnConflictBusinessEvent(const QnBusiness::EventType eventType,
                                     const QnResourcePtr& resource,
                                     const qint64 timeStamp,
                                     const QString& source = QString(),
                                     const QStringList& conflicts = QStringList());
    
    virtual QnBusinessEventParameters getRuntimeParams() const override;
protected:
    QString m_source;
    QStringList m_conflicts;
};

#endif // CONFLICT_BUSINESS_EVENT_H
