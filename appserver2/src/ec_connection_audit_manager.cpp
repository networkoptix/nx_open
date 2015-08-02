#include "ec_connection_audit_manager.h"
#include "audit/audit_manager.h"
#include "utils/common/synctime.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/user_resource.h"

namespace ec2
{

    ECConnectionAuditManager::ECConnectionAuditManager(AbstractECConnection* ecConnection)
    {
        Q_UNUSED(ecConnection)
    }

    void ECConnectionAuditManager::addAuditRecord( const ApiCameraAttributesDataList& params, const QnAuthSession& authInfo)
    {
        QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_CameraUpdate);
        for (const auto& value: params)
            auditRecord.resources.push_back(value.cameraID);
        qnAuditManager->addAuditRecord(auditRecord);
    }

    void ECConnectionAuditManager::addAuditRecord( const ApiCameraAttributesData& params, const QnAuthSession& authInfo)
    {
        QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_CameraUpdate);
        auditRecord.resources.push_back(params.cameraID);
        qnAuditManager->addAuditRecord(auditRecord);
    }

    void ECConnectionAuditManager::addAuditRecord( const ApiMediaServerUserAttributesData& params, const QnAuthSession& authInfo)
    {
        QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_ServerUpdate);
        auditRecord.resources.push_back(params.serverID);
        qnAuditManager->addAuditRecord(auditRecord);
    }

    void ECConnectionAuditManager::addAuditRecord( const ApiMediaServerUserAttributesDataList& params, const QnAuthSession& authInfo)
    {
        QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_ServerUpdate);
        for (const auto& value: params)
            auditRecord.resources.push_back(value.serverID);
        qnAuditManager->addAuditRecord(auditRecord);
    }
    
    void ECConnectionAuditManager::addAuditRecord( const ApiUserDataList& params, const QnAuthSession& authInfo)
    {
        QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_UserUpdate);
        for (const auto& value: params)
            auditRecord.resources.push_back(value.id);
        qnAuditManager->addAuditRecord(auditRecord);
    }

    void ECConnectionAuditManager::addAuditRecord( const ApiUserData& params, const QnAuthSession& authInfo)
    {
        QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_UserUpdate);
        auditRecord.resources.push_back(params.id);
        qnAuditManager->addAuditRecord(auditRecord);
    }

    void ECConnectionAuditManager::addAuditRecord( const ApiBusinessRuleData& params, const QnAuthSession& authInfo)
    {
        QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_BEventUpdate);
        auditRecord.resources.push_back(params.id);
        qnAuditManager->addAuditRecord(auditRecord);
    }

    void ECConnectionAuditManager::addAuditRecord( const ApiBusinessRuleDataList& params, const QnAuthSession& authInfo)
    {
        QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_BEventUpdate);
        for (const auto& value: params)
            auditRecord.resources.push_back(value.id);
        qnAuditManager->addAuditRecord(auditRecord);
    }

    void ECConnectionAuditManager::addAuditRecord( const ApiResourceParamWithRefDataList& params, const QnAuthSession& authInfo)
    {
        QnUserResourcePtr adminUser = qnResPool->getAdministrator();
        if (!adminUser)
            return;
        QnUuid adminId = adminUser->getId();
        for (const ApiResourceParamWithRefData& param: params)
        {
            if (param.resourceId == adminId) 
                qnAuditManager->notifySettingsChanged(authInfo, param.name);
        }
    }
}
