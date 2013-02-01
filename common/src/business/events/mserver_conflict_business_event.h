#ifndef __MSERVER_CONFLICT_BUSINESS_EVENT_H__
#define __MSERVER_CONFLICT_BUSINESS_EVENT_H__

#include "instant_business_event.h"
#include "core/resource/resource_fwd.h"

namespace QnBusinessEventRuntime {
    QString getCurrentEC(const QnBusinessParams &params);
    void setCurrentEC(QnBusinessParams* params, QString value);

    QStringList getConflictECs(const QnBusinessParams &params);
    void setConflictECs(QnBusinessParams* params, QStringList value);
}

class QnMServerConflictBusinessEvent: public QnInstantBusinessEvent
{
    typedef QnInstantBusinessEvent base_type;
public:
    QnMServerConflictBusinessEvent(const QnResourcePtr& mServerRes, qint64 timeStamp, const QList<QByteArray>& conflictList);

    virtual QnBusinessParams getRuntimeParams() const override;
    virtual QString toString() const override;
private:
    QList<QByteArray> m_conflictList;
};

typedef QSharedPointer<QnMServerConflictBusinessEvent> QnMServerConflictBusinessEventPtr;

#endif // __MSERVER_CONFLICT_BUSINESS_EVENT_H__
