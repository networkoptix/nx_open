#ifndef __STORAGE_FAILURE_BUSINESS_EVENT_H__
#define __STORAGE_FAILURE_BUSINESS_EVENT_H__

#include "instant_business_event.h"

class QnStorageFailureBusinessEvent: public QnInstantBusinessEvent
{
    typedef QnInstantBusinessEvent base_type;
public:
    QnStorageFailureBusinessEvent(const QnResourcePtr& resource,
                          qint64 timeStamp,
                          QnResourcePtr storageResource,
                          const QString& reason);

    virtual QString toString() const override;
private:
    QString m_reason;
    QnResourcePtr m_storageResource;
};

typedef QSharedPointer<QnStorageFailureBusinessEvent> QnStorageFailureBusinessEventPtr;

#endif // __STORAGE_FAILURE_BUSINESS_EVENT_H__
