#ifndef __STORAGE_FAILURE_BUSINESS_EVENT_H__
#define __STORAGE_FAILURE_BUSINESS_EVENT_H__

#include "abstract_business_event.h"
#include "core/datapacket/media_data_packet.h"

class QnStorageFailureBusinessEvent: public QnAbstractBusinessEvent
{
public:
    QnStorageFailureBusinessEvent(QnResourcePtr resource,
                          qint64 timeStamp);
};

typedef QSharedPointer<QnStorageFailureBusinessEvent> QnStorageFailureBusinessEventPtr;

#endif // __STORAGE_FAILURE_BUSINESS_EVENT_H__
