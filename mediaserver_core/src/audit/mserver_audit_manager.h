#ifndef __MSERVER_AUDIT_MANAGER_H__
#define __MSERVER_AUDIT_MANAGER_H__

#include "audit/audit_manager.h"

class QnMServerAuditManager: public QnAuditManager
{
public:
    QnMServerAuditManager();
    virtual void addAuditRecord(const QnAuditRecord& record) override;
};

#endif // __MSERVER_AUDIT_MANAGER_H__
