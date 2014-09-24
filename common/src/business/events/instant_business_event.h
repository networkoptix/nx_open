#ifndef INSTANT_BUSINESS_EVENT_H
#define INSTANT_BUSINESS_EVENT_H

#include "abstract_business_event.h"

class QnInstantBusinessEvent: public QnAbstractBusinessEvent
{
    typedef QnAbstractBusinessEvent base_type;
protected:
    explicit QnInstantBusinessEvent(QnBusiness::EventType eventType,
                                    const QnResourcePtr& resource,
                                    qint64 timeStamp);
public:
    virtual bool checkCondition(QnBusiness::EventState state, const QnBusinessEventParameters &params) const override;
};

#endif // INSTANT_BUSINESS_EVENT_H
