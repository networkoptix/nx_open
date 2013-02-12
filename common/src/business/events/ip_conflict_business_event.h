#ifndef __IP_CONFLICT_BUSINESS_EVENT_H__
#define __IP_CONFLICT_BUSINESS_EVENT_H__

#include <QHostAddress>

#include "instant_business_event.h"
#include "core/resource/resource_fwd.h"

namespace QnBusinessEventRuntime {
    QHostAddress getCameraAddress(const QnBusinessParams &params);
    void setCameraAddress(QnBusinessParams* params, QHostAddress value);

    QVariantList getConflictingCamerasMac(const QnBusinessParams &params);
    void setConflictingCamerasMac(QnBusinessParams* params, QVariantList value);
}

class QnIPConflictBusinessEvent: public QnInstantBusinessEvent
{
    typedef QnInstantBusinessEvent base_type;
public:
    QnIPConflictBusinessEvent(const QnResourcePtr& resource, const QHostAddress& address, const QStringList& cameras,  qint64 timeStamp);
    virtual QString toString() const override;

    virtual QnBusinessParams getRuntimeParams() const override;
private:
    QHostAddress m_address;
    QStringList m_cameras;
};

typedef QSharedPointer<QnIPConflictBusinessEvent> QnIPConflictBusinessEventPtr;

#endif // __IP_CONFLICT_BUSINESS_EVENT_H__
