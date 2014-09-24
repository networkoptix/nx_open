#ifndef __STORAGE_FAILURE_BUSINESS_EVENT_H__
#define __STORAGE_FAILURE_BUSINESS_EVENT_H__

#include "reasoned_business_event.h"

class QnStorageFailureBusinessEvent: public QnReasonedBusinessEvent
{
    typedef QnReasonedBusinessEvent base_type;
public:
    QnStorageFailureBusinessEvent(const QnResourcePtr& resource,
                          qint64 timeStamp,
                          QnBusiness::EventReason reasonCode,
                          const QnResourcePtr& storageResource
                          );

};

typedef QSharedPointer<QnStorageFailureBusinessEvent> QnStorageFailureBusinessEventPtr;

#endif // __STORAGE_FAILURE_BUSINESS_EVENT_H__
