#ifndef PROLONGED_BUSINESS_EVENT_H
#define PROLONGED_BUSINESS_EVENT_H

#include "abstract_business_event.h"

class QnProlongedBusinessEvent: public QnAbstractBusinessEvent
{
    typedef QnAbstractBusinessEvent base_type;
protected:
    explicit QnProlongedBusinessEvent(QnBusiness::EventType eventType, const QnResourcePtr& resource, QnBusiness::EventState toggleState, qint64 timeStamp);
public:
    virtual bool checkCondition(QnBusiness::EventState state, const QnBusinessEventParameters &params) const override;
};

#endif // PROLONGED_BUSINESS_EVENT_H
