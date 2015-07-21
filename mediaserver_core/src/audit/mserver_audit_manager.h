#ifndef __MSERVER_AUDIT_MANAGER_H__
#define __MSERVER_AUDIT_MANAGER_H__

#include "audit/audit_manager.h"
#include "nx_ec/data/api_server_alive_data.h"

class QnMServerAuditManager: public QnAuditManager
{
public:
    QnMServerAuditManager();
    ~QnMServerAuditManager();
    virtual int addAuditRecord(const QnAuditRecord& record) override;
    virtual int updateAuditRecord(int internalId, const QnAuditRecord& record) override;
//private slots:
    
    //void at_remotePeerLost(const ec2::ApiPeerAliveData &data);
};

#endif // __MSERVER_AUDIT_MANAGER_H__
