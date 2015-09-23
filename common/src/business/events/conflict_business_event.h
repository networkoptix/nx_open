#ifndef CONFLICT_BUSINESS_EVENT_H
#define CONFLICT_BUSINESS_EVENT_H

#include "instant_business_event.h"
#include "core/resource/resource_fwd.h"

class QnConflictBusinessEvent : public QnInstantBusinessEvent
{
    typedef QnInstantBusinessEvent base_type;
public:
    static QString encodeList(const QStringList& value);

    static const QChar Delimiter;

    explicit QnConflictBusinessEvent(const QnBusiness::EventType eventType,
                                     const QnResourcePtr& resource,
                                     const qint64 timeStamp,
                                     const QString& caption = QString(),
                                     const QString& description = QString());
    
    virtual QnBusinessEventParameters getRuntimeParams() const override;
protected:
    QString m_caption;
    QString m_description;
};

#endif // CONFLICT_BUSINESS_EVENT_H
