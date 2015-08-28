#ifndef CUSTOM_BUSINESS_EVENT_H
#define CUSTOM_BUSINESS_EVENT_H

#include <business/events/prolonged_business_event.h>
#include <core/resource/resource_fwd.h>

class QnCustomBusinessEvent: public QnAbstractBusinessEvent {
    typedef QnAbstractBusinessEvent base_type;
public:
    QnCustomBusinessEvent(QnBusiness::EventState toggleState, qint64 timeStamp, const QString& resourceName, const QString& caption, const QString& description);

    virtual bool checkCondition(QnBusiness::EventState state, const QnBusinessEventParameters &params) const override;

    virtual QnBusinessEventParameters getRuntimeParams() const override;
private:
    const QString m_resourceName;
    const QString m_caption;
    const QString m_description;
};

typedef QSharedPointer<QnCustomBusinessEvent> QnCustomBusinessEventPtr;

#endif // CUSTOM_BUSINESS_EVENT_H
