#include "ec_connection_audit_manager.h"
#include "audit/audit_manager.h"
#include "utils/common/synctime.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/user_resource.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/camera_resource.h"
#include "api/common_message_processor.h"
#include "business/business_strings_helper.h"

namespace ec2
{

    ECConnectionAuditManager::ECConnectionAuditManager(AbstractECConnection* ecConnection)
    {
        Q_UNUSED(ecConnection)
    }

    void ECConnectionAuditManager::addAuditRecord(ApiCommand::Value command,  const ApiCameraAttributesDataList& params, const QnAuthSession& authInfo)
    {
        Q_UNUSED(command);
        QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_CameraUpdate);
        for (const auto& value: params)
            auditRecord.resources.push_back(value.cameraID);
        qnAuditManager->addAuditRecord(auditRecord);
    }

    void ECConnectionAuditManager::addAuditRecord(ApiCommand::Value command,  const ApiCameraAttributesData& params, const QnAuthSession& authInfo)
    {
        Q_UNUSED(command);
        QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_CameraUpdate);
        auditRecord.resources.push_back(params.cameraID);
        qnAuditManager->addAuditRecord(auditRecord);
    }

    void ECConnectionAuditManager::addAuditRecord(ApiCommand::Value command,  const ApiMediaServerUserAttributesData& params, const QnAuthSession& authInfo)
    {
        Q_UNUSED(command);
        QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_ServerUpdate);
        auditRecord.resources.push_back(params.serverID);
        qnAuditManager->addAuditRecord(auditRecord);
    }

    void ECConnectionAuditManager::addAuditRecord(ApiCommand::Value command,  const ApiMediaServerUserAttributesDataList& params, const QnAuthSession& authInfo)
    {
        Q_UNUSED(command);
        QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_ServerUpdate);
        for (const auto& value: params)
            auditRecord.resources.push_back(value.serverID);
        qnAuditManager->addAuditRecord(auditRecord);
    }
    
    void ECConnectionAuditManager::addAuditRecord(ApiCommand::Value command,  const ApiUserDataList& params, const QnAuthSession& authInfo)
    {
        Q_UNUSED(command);
        QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_UserUpdate);
        for (const auto& value: params)
            auditRecord.resources.push_back(value.id);
        qnAuditManager->addAuditRecord(auditRecord);
    }

    void ECConnectionAuditManager::addAuditRecord(ApiCommand::Value command,  const ApiUserData& params, const QnAuthSession& authInfo)
    {
        Q_UNUSED(command);
        QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_UserUpdate);
        auditRecord.resources.push_back(params.id);
        qnAuditManager->addAuditRecord(auditRecord);
    }

    void ECConnectionAuditManager::addAuditRecord(ApiCommand::Value command,  const ApiBusinessRuleData& params, const QnAuthSession& authInfo)
    {
        Q_UNUSED(command);
        QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_BEventUpdate);
        auditRecord.resources.push_back(params.id);
        qnAuditManager->addAuditRecord(auditRecord);
    }

    void ECConnectionAuditManager::addAuditRecord(ApiCommand::Value command,  const ApiBusinessRuleDataList& params, const QnAuthSession& authInfo)
    {
        Q_UNUSED(command);
        QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_BEventUpdate);
        for (const auto& value: params)
            auditRecord.resources.push_back(value.id);
        qnAuditManager->addAuditRecord(auditRecord);
    }

    void ECConnectionAuditManager::addAuditRecord(ApiCommand::Value command,  const ApiResourceParamWithRefData& param, const QnAuthSession& authInfo)
    {
        Q_UNUSED(command);
        QnUserResourcePtr adminUser = qnResPool->getAdministrator();
        if (!adminUser)
            return;
        QnUuid adminId = adminUser->getId();
        if (param.resourceId == adminId) 
            qnAuditManager->notifySettingsChanged(authInfo, param.name);
    }

    void ECConnectionAuditManager::addAuditRecord(ApiCommand::Value command,  const ApiResourceParamWithRefDataList& params, const QnAuthSession& authInfo)
    {
        Q_UNUSED(command);
        for (const ApiResourceParamWithRefData& param: params)
            addAuditRecord(command, param, authInfo);
    }
    
    void ECConnectionAuditManager::addAuditRecord(ApiCommand::Value command,  const ApiIdData& params, const QnAuthSession& authInfo)
    {
        Qn::AuditRecordType eventType = Qn::AR_NotDefined;
        QString description;
        switch(command)
        {
            case ApiCommand::removeResource:
            case ApiCommand::removeResources:
            case ApiCommand::removeCamera:
            case ApiCommand::removeMediaServer:
            case ApiCommand::removeUser:
            {
                if (QnResourcePtr res = qnResPool->getResourceById(params.id)) 
                {
                    description = res->getName();
                    if (res.dynamicCast<QnUserResource>())
                        eventType = Qn::AR_UserRemove;
                    else if (res.dynamicCast<QnSecurityCamResource>())
                        eventType = Qn::AR_CameraRemove;
                    else if (res.dynamicCast<QnMediaServerResource>())
                        eventType = Qn::AR_ServerRemove;
                }
                break;
            }
            case ApiCommand::removeBusinessRule:
            {
                eventType = Qn::AR_BEventRemove;
                auto msgProc = QnCommonMessageProcessor::instance();
                if (msgProc) {
                    QnBusinessEventRulePtr bRule = msgProc->businessRules().value(params.id);
                    if (bRule) 
                        description = QnBusinessStringsHelper::bruleDescriptionText(bRule);
                }
                break;
            }
        }

        if (eventType != Qn::AR_NotDefined) {
            auto auditRecord = qnAuditManager->prepareRecord(authInfo, eventType);
            qnAuditManager->addAuditRecord(auditRecord);
        }
    }

    void ECConnectionAuditManager::addAuditRecord(ApiCommand::Value command,  const ApiIdDataList& params, const QnAuthSession& authInfo)
    {
        for (const ApiIdData& param: params)
            addAuditRecord(command, param, authInfo);
    }
    
    void ECConnectionAuditManager::addAuditRecord(ApiCommand::Value command,  const ApiResetBusinessRuleData& params, const QnAuthSession& authInfo)
    {
        Q_UNUSED(command);
        Q_UNUSED(params);
        qnAuditManager->addAuditRecord(qnAuditManager->prepareRecord(authInfo, Qn::AR_BEventReset));
    }

    void ECConnectionAuditManager::addAuditRecord(ApiCommand::Value command,  const ApiDatabaseDumpData& params, const QnAuthSession& authInfo)
    {
        Q_UNUSED(command);
        Q_UNUSED(params);
        qnAuditManager->addAuditRecord(qnAuditManager->prepareRecord(authInfo, Qn::AR_DatabaseRestore));
    }
}
