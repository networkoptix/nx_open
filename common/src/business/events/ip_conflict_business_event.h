#ifndef __IP_CONFLICT_BUSINESS_EVENT_H__
#define __IP_CONFLICT_BUSINESS_EVENT_H__

#include <QHostAddress>

#include "instant_business_event.h"
#include "core/resource/resource_fwd.h"

namespace QnBusinessEventRuntime {
    QHostAddress getCameraAddress(const QnBusinessParams &params);
    void setCameraAddress(QnBusinessParams* params, QHostAddress value);

    QStringList getConflictingMacAddrs(const QnBusinessParams &params);
    void setConflictingMacAddrs(QnBusinessParams* params, QStringList value);
}

class QnIPConflictBusinessEvent: public QnInstantBusinessEvent
{
    typedef QnInstantBusinessEvent base_type;
public:
    QnIPConflictBusinessEvent(const QnResourcePtr& resource, const QHostAddress& address, const QStringList& macAddrList,  qint64 timeStamp);
    virtual QString toString() const override;

    virtual QnBusinessParams getRuntimeParams() const override;
private:
    QHostAddress m_address;
    QStringList m_macAddrList;
};

typedef QSharedPointer<QnIPConflictBusinessEvent> QnIPConflictBusinessEventPtr;

#endif // __IP_CONFLICT_BUSINESS_EVENT_H__
