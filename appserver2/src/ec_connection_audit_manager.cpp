#include "ec_connection_audit_manager.h"

#include <audit/audit_manager.h>
#include <utils/common/synctime.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <api/common_message_processor.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <api/global_settings.h>
#include <common/common_module.h>
#include <nx_ec/ec_api.h>

namespace ec2 {


ECConnectionAuditManager::ECConnectionAuditManager(AbstractECConnection* ecConnection):
    m_connection(ecConnection)
{
    connect(
        ecConnection->getMediaServerNotificationManager().get(),
        &ec2::AbstractMediaServerNotificationManager::userAttributesRemoved,
        this,
        &ECConnectionAuditManager::at_resourceAboutToRemoved,
        Qt::DirectConnection);

    connect(
        ecConnection->getCameraNotificationManager().get(),
        &ec2::AbstractCameraNotificationManager::userAttributesRemoved,
        this,
        &ECConnectionAuditManager::at_resourceAboutToRemoved,
        Qt::DirectConnection);
}

ECConnectionAuditManager::~ECConnectionAuditManager()
{
    m_connection->getMediaServerNotificationManager()->disconnect(this);
    m_connection->getCameraNotificationManager()->disconnect(this);
}

void ECConnectionAuditManager::at_resourceAboutToRemoved(const QnUuid& id)
{
    if (QnResourcePtr res = m_connection->commonModule()->resourcePool()->getResourceById(id))
        m_removedResourceNames[id] = res->getName();
}

void ECConnectionAuditManager::addAuditRecord(ApiCommand::Value command,  const ApiCameraAttributesDataList& params, const QnAuthSession& authInfo)
{
    Q_UNUSED(command);
    QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_CameraUpdate);
    for (const auto& value: params)
        auditRecord.resources.push_back(value.cameraId);
    qnAuditManager->addAuditRecord(auditRecord);
}

void ECConnectionAuditManager::addAuditRecord(ApiCommand::Value command,  const ApiCameraAttributesData& params, const QnAuthSession& authInfo)
{
    Q_UNUSED(command);
    QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_CameraUpdate);
    auditRecord.resources.push_back(params.cameraId);
    qnAuditManager->addAuditRecord(auditRecord);
}

void ECConnectionAuditManager::addAuditRecord(ApiCommand::Value command,  const ApiMediaServerUserAttributesData& params, const QnAuthSession& authInfo)
{
    Q_UNUSED(command);
    QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_ServerUpdate);
    auditRecord.resources.push_back(params.serverId);
    qnAuditManager->addAuditRecord(auditRecord);
}

void ECConnectionAuditManager::addAuditRecord(ApiCommand::Value command,  const ApiMediaServerUserAttributesDataList& params, const QnAuthSession& authInfo)
{
    Q_UNUSED(command);
    QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_ServerUpdate);
    for (const auto& value: params)
        auditRecord.resources.push_back(value.serverId);
    qnAuditManager->addAuditRecord(auditRecord);
}

void ECConnectionAuditManager::addAuditRecord(ApiCommand::Value command, const ApiStorageData& params, const QnAuthSession& authInfo)
{
    Q_UNUSED(command);
    QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_ServerUpdate);
    auditRecord.resources.push_back(params.parentId);
    qnAuditManager->addAuditRecord(auditRecord);
}

void ECConnectionAuditManager::addAuditRecord(ApiCommand::Value command, const ApiStorageDataList& params, const QnAuthSession& authInfo)
{
    Q_UNUSED(command);
    QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_ServerUpdate);
    for (const auto& value: params)
        auditRecord.resources.push_back(value.parentId);
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
    nx::vms::event::RulePtr rule(new nx::vms::event::Rule());
    fromApiToResource(params, rule);
    nx::vms::event::StringsHelper helper(m_connection->commonModule());
    auditRecord.addParam("description", helper.ruleDescriptionText(rule).toUtf8());

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
    if (QnGlobalSettings::isGlobalSetting(param))
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
    const auto& resPool = m_connection->commonModule()->resourcePool();
    Qn::AuditRecordType eventType = Qn::AR_NotDefined;
    QString description;
    QnUuid resourceId;
    switch(command)
    {
        case ApiCommand::removeStorage:
            if (QnResourcePtr res = resPool->getResourceById(params.id))
            {
                eventType = Qn::AR_ServerUpdate;
                resourceId = res->getParentId();
            }
            break;
        case ApiCommand::removeResource:
        case ApiCommand::removeResources:
        case ApiCommand::removeCamera:
        case ApiCommand::removeMediaServer:
        case ApiCommand::removeUser:
        {
            if (QnResourcePtr res = resPool->getResourceById(params.id))
            {
                description = m_removedResourceNames.value(params.id);
                if (description.isNull())
                    description = res->getName();
                if (res.dynamicCast<QnUserResource>())
                    eventType = Qn::AR_UserRemove;
                else if (res.dynamicCast<QnSecurityCamResource>()) {
                    eventType = Qn::AR_CameraRemove;
                    if (QnSecurityCamResourcePtr camRes = res.dynamicCast<QnSecurityCamResource>())
                        description = lit("%1 (%2)").arg(description).arg(camRes->getHostAddress());
                }
                else if (res.dynamicCast<QnMediaServerResource>())
                    eventType = Qn::AR_ServerRemove;
            }
            break;
        }
        case ApiCommand::removeEventRule:
        {
            eventType = Qn::AR_BEventRemove;
            auto ruleManager = m_connection->commonModule()->eventRuleManager();
            if (ruleManager) {
                nx::vms::event::RulePtr rule = ruleManager->rule(params.id);
                if (rule)
                {
                    nx::vms::event::StringsHelper helper(m_connection->commonModule());
                    description = helper.ruleDescriptionText(rule);
                }
            }
            break;
        }
        default:
        {
            // do not audit these commands
            break;
        }
    }

    if (eventType != Qn::AR_NotDefined)
    {
        auto auditRecord = qnAuditManager->prepareRecord(authInfo, eventType);
        if (!description.isEmpty())
            auditRecord.addParam("description", description.toUtf8());
        if (!resourceId.isNull())
            auditRecord.resources.push_back(resourceId);
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

AbstractECConnection* ECConnectionAuditManager::ec2Connection() const
{
    return m_connection;
}

} // namespace ec2
