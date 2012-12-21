#ifndef __STORAGE_FAILURE_BUSINESS_EVENT_H__
#define __STORAGE_FAILURE_BUSINESS_EVENT_H__

#include "instant_business_event.h"

class QnStorageFailureBusinessEvent: public QnInstantBusinessEvent
{
    typedef QnInstantBusinessEvent base_type;
public:
    QnStorageFailureBusinessEvent(const QnResourcePtr& resource,
                          qint64 timeStamp);

    virtual QString toString() const override;
private:
};

typedef QSharedPointer<QnStorageFailureBusinessEvent> QnStorageFailureBusinessEventPtr;

#endif // __STORAGE_FAILURE_BUSINESS_EVENT_H__
