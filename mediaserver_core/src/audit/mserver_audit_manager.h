#ifndef __MSERVER_AUDIT_MANAGER_H__
#define __MSERVER_AUDIT_MANAGER_H__

#include <QElapsedTimer>
#include "audit/audit_manager.h"
#include "nx_ec/data/api_server_alive_data.h"

class QnMServerAuditManager: public QnAuditManager
{
public:
    QnMServerAuditManager();
    ~QnMServerAuditManager();
protected:
    virtual int addAuditRecordInternal(const QnAuditRecord& record) override;
    virtual int updateAuditRecordInternal(int internalId, const QnAuditRecord& record) override;
private:
    //QHash<QnUuid, qint64> m_knownSessions;
    mutable QMutex m_mutex;
};

#endif // __MSERVER_AUDIT_MANAGER_H__
