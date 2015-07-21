#include "ec_connection_audit_manager.h"
#include "audit/audit_manager.h"
#include "utils/common/synctime.h"

namespace ec2
{
    ECConnectionAuditManager::ECConnectionAuditManager(AbstractECConnection* ecConnection)
    {
        Q_UNUSED(ecConnection)
    }

    void ECConnectionAuditManager::addAuditRecord( const ApiCameraAttributesDataList& params, const QnAuthSession& authInfo)
    {
        if (!qnAuditManager)
            return;

        QnAuditRecord auditRecord;
        auditRecord.fillAuthInfo(authInfo);
        auditRecord.timestamp = qnSyncTime->currentMSecsSinceEpoch() / 1000;
        auditRecord.eventType = Qn::AR_CameraUpdate;
        for (const auto& value: params)
            auditRecord.resources.push_back(value.cameraID);

        qnAuditManager->addAuditRecord(auditRecord);
    }

    void ECConnectionAuditManager::addAuditRecord( const ApiCameraAttributesData& params, const QnAuthSession& authInfo)
    {
        if (!qnAuditManager)
            return;
        
        QnAuditRecord auditRecord;
        auditRecord.fillAuthInfo(authInfo);
        auditRecord.timestamp = qnSyncTime->currentMSecsSinceEpoch() / 1000;
        auditRecord.eventType = Qn::AR_CameraUpdate;
        auditRecord.resources.push_back(params.cameraID);
        
        qnAuditManager->addAuditRecord(auditRecord);
    }
}
