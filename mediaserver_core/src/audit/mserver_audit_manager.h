#ifndef __MSERVER_AUDIT_MANAGER_H__
#define __MSERVER_AUDIT_MANAGER_H__

#include <QElapsedTimer>

#include <nx/utils/thread/mutex.h>

#include "audit/audit_manager.h"
#include "nx_ec/data/api_peer_alive_data.h"

class QnMServerAuditManager: public QnAuditManager
{
public:
    QnMServerAuditManager(std::chrono::milliseconds lastRunningTime, QObject* parent);

protected:
    virtual int addAuditRecordInternal(const QnAuditRecord& record) override;
    virtual int updateAuditRecordInternal(int internalId, const QnAuditRecord& record) override;

private:
    QTimer m_timer;
    mutable QnMutex m_mutex;
    std::map<int, QnAuditRecord> m_recordsToAdd;
    int m_internalId;
};

#endif // __MSERVER_AUDIT_MANAGER_H__
