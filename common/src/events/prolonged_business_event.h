#ifndef PROLONGED_BUSINESS_EVENT_H
#define PROLONGED_BUSINESS_EVENT_H

#include "abstract_business_event.h"

class QnProlongedBusinessEvent: public QnAbstractBusinessEvent
{
    typedef QnAbstractBusinessEvent base_type;
protected:
    explicit QnProlongedBusinessEvent(BusinessEventType::Value eventType,
                                    const QnResourcePtr& resource,
                                    ToggleState::Value toggleState,
                                    qint64 timeStamp);
public:
    virtual bool checkCondition(const QnBusinessParams &params) const override;
};

#endif // PROLONGED_BUSINESS_EVENT_H
