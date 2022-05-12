// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ec_connection_audit_manager.h"

#include <api/common_message_processor.h>
#include <audit/audit_manager.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_camera_manager.h>
#include <nx_ec/managers/abstract_media_server_manager.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/event/rule_manager.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/strings_helper.h>
#include <utils/common/synctime.h>

namespace ec2 {

using namespace nx::vms::api;

ECConnectionAuditManager::ECConnectionAuditManager(
    AbstractECConnection* ecConnection,
    QnCommonModule* commonModule)
    :
    QnCommonModuleAware(commonModule),
    m_connection(ecConnection)
{
    connect(
        ecConnection->mediaServerNotificationManager().get(),
        &ec2::AbstractMediaServerNotificationManager::userAttributesRemoved,
        this,
        &ECConnectionAuditManager::at_resourceAboutToRemoved,
        Qt::DirectConnection);

    connect(
        ecConnection->cameraNotificationManager().get(),
        &ec2::AbstractCameraNotificationManager::userAttributesRemoved,
        this,
        &ECConnectionAuditManager::at_resourceAboutToRemoved,
        Qt::DirectConnection);
}

ECConnectionAuditManager::~ECConnectionAuditManager()
{
    m_connection->mediaServerNotificationManager()->disconnect(this);
    m_connection->cameraNotificationManager()->disconnect(this);
}

void ECConnectionAuditManager::at_resourceAboutToRemoved(const QnUuid& id)
{
    if (QnResourcePtr res = resourcePool()->getResourceById(id))
        m_removedResourceNames[id] = res->getName();
}

void ECConnectionAuditManager::addAuditRecord(
    ApiCommand::Value /*command*/,
    const CameraData& params,
    const QnAuthSession& authInfo)
{
    if (resourcePool()->getResourceById(params.id))
        return; //< Don't log if resource already exists.
    QnAuditRecord auditRecord = commonModule()->auditManager()->prepareRecord(authInfo, Qn::AR_CameraInsert);
    auditRecord.resources.push_back(params.id);
    commonModule()->auditManager()->addAuditRecord(auditRecord);
}

void ECConnectionAuditManager::addAuditRecord(
    ApiCommand::Value /*command*/,
    const CameraAttributesDataList& params,
    const QnAuthSession& authInfo)
{
    QnAuditRecord auditRecord = commonModule()->auditManager()->prepareRecord(authInfo, Qn::AR_CameraUpdate);
    for (const auto& value: params)
        auditRecord.resources.push_back(value.cameraId);
    commonModule()->auditManager()->addAuditRecord(auditRecord);
}

void ECConnectionAuditManager::addAuditRecord(
    ApiCommand::Value /*command*/,
    const CameraAttributesData& params,
    const QnAuthSession& authInfo)
{
    QnAuditRecord auditRecord = commonModule()->auditManager()->prepareRecord(authInfo, Qn::AR_CameraUpdate);
    auditRecord.resources.push_back(params.cameraId);
    commonModule()->auditManager()->addAuditRecord(auditRecord);
}

void ECConnectionAuditManager::addAuditRecord(
    ApiCommand::Value /*command*/,
    const MediaServerUserAttributesData& params,
    const QnAuthSession& authInfo)
{
    QnAuditRecord auditRecord = commonModule()->auditManager()->prepareRecord(authInfo, Qn::AR_ServerUpdate);
    auditRecord.resources.push_back(params.serverId);
    commonModule()->auditManager()->addAuditRecord(auditRecord);
}

void ECConnectionAuditManager::addAuditRecord(
    ApiCommand::Value /*command*/,
    const MediaServerUserAttributesDataList& params,
    const QnAuthSession& authInfo)
{
    QnAuditRecord auditRecord = commonModule()->auditManager()->prepareRecord(authInfo, Qn::AR_ServerUpdate);
    for (const auto& value: params)
        auditRecord.resources.push_back(value.serverId);
    commonModule()->auditManager()->addAuditRecord(auditRecord);
}

void ECConnectionAuditManager::addAuditRecord(
    ApiCommand::Value /*command*/,
    const StorageData& params,
    const QnAuthSession& authInfo)
{
    auto command = resourcePool()->getResourceById(params.id)
        ? Qn::AR_StorageUpdate : Qn::AR_StorageInsert;
    QnAuditRecord auditRecord = commonModule()->auditManager()->prepareRecord(authInfo, command);
    auditRecord.resources.push_back(params.id);
    commonModule()->auditManager()->addAuditRecord(auditRecord);
}

void ECConnectionAuditManager::addAuditRecord(
    ApiCommand::Value /*command*/,
    const StorageDataList& params,
    const QnAuthSession& authInfo)
{
    QnAuditRecord auditRecord = commonModule()->auditManager()->prepareRecord(authInfo, Qn::AR_StorageUpdate);
    for (const auto& value: params)
        auditRecord.resources.push_back(value.id);
    commonModule()->auditManager()->addAuditRecord(auditRecord);
}

void ECConnectionAuditManager::addAuditRecord(
    ApiCommand::Value /*command*/,
    const UserDataList& params,
    const QnAuthSession& authInfo)
{
    QnAuditRecord auditRecord = commonModule()->auditManager()->prepareRecord(authInfo, Qn::AR_UserUpdate);
    for (const auto& value: params)
        auditRecord.resources.push_back(value.id);
    commonModule()->auditManager()->addAuditRecord(auditRecord);
}

void ECConnectionAuditManager::addAuditRecord(
    ApiCommand::Value /*command*/,
    const UserData& params,
    const QnAuthSession& authInfo)
{
    QnAuditRecord auditRecord = commonModule()->auditManager()->prepareRecord(authInfo, Qn::AR_UserUpdate);
    auditRecord.resources.push_back(params.id);
    commonModule()->auditManager()->addAuditRecord(auditRecord);
}

void ECConnectionAuditManager::addAuditRecord(
    ApiCommand::Value /*command*/,
    const EventRuleData& params,
    const QnAuthSession& authInfo)
{
    QnAuditRecord auditRecord = commonModule()->auditManager()->prepareRecord(authInfo, Qn::AR_BEventUpdate);
    auditRecord.resources.push_back(params.id);
    nx::vms::event::RulePtr rule(new nx::vms::event::Rule());
    fromApiToResource(params, rule);
    nx::vms::event::StringsHelper helper(commonModule()->systemContext());
    auditRecord.addParam("description", helper.ruleDescriptionText(rule).toUtf8());

    commonModule()->auditManager()->addAuditRecord(auditRecord);
}

void ECConnectionAuditManager::addAuditRecord(
    ApiCommand::Value /*command*/,
    const EventRuleDataList& params,
    const QnAuthSession& authInfo)
{
    QnAuditRecord auditRecord = commonModule()->auditManager()->prepareRecord(authInfo, Qn::AR_BEventUpdate);
    for (const auto& value: params)
        auditRecord.resources.push_back(value.id);
    commonModule()->auditManager()->addAuditRecord(auditRecord);
}

void ECConnectionAuditManager::addAuditRecord(
    ApiCommand::Value command,
    const ResourceParamWithRefDataList& params,
    const QnAuthSession& authInfo)
{
    std::map<QString, QString> settings;
    for (const auto& param: params)
    {
        if (nx::vms::common::SystemSettings::isGlobalSetting(param))
            settings[param.name] = param.value;
    }
    commonModule()->auditManager()->notifySettingsChanged(authInfo, std::move(settings));
}

void ECConnectionAuditManager::addAuditRecord(
    ApiCommand::Value command,
    const IdData& params,
    const QnAuthSession& authInfo)
{
    const auto& resPool = resourcePool();
    Qn::AuditRecordType eventType = Qn::AR_NotDefined;
    QString description;
    QnUuid resourceId;
    switch(command)
    {
        case ApiCommand::removeStorage:
        case ApiCommand::removeResource:
        case ApiCommand::removeResources:
        case ApiCommand::removeCamera:
        case ApiCommand::removeMediaServer:
        case ApiCommand::removeUser:
        {
            if (QnResourcePtr res = resPool->getResourceById(params.id))
            {
                resourceId = params.id;
                description = m_removedResourceNames.value(params.id);
                if (description.isNull())
                    description = res->getName();
                if (res.dynamicCast<QnUserResource>())
                    eventType = Qn::AR_UserRemove;
                else if (res.dynamicCast<QnSecurityCamResource>())
                {
                    eventType = Qn::AR_CameraRemove;
                    if (QnSecurityCamResourcePtr camRes = res.dynamicCast<QnSecurityCamResource>())
                        description = lit("%1 (%2)").arg(description).arg(
                            camRes->getHostAddress());
                }
                else if (res.dynamicCast<QnMediaServerResource>())
                    eventType = Qn::AR_ServerRemove;
                else if (auto storage = res.dynamicCast<QnStorageResource>())
                {
                    eventType = Qn::AR_StorageRemove;
                    description = storage->urlWithoutCredentials();
                }
            }
            break;
        }
        case ApiCommand::removeEventRule:
        {
            eventType = Qn::AR_BEventRemove;
            auto ruleManager = eventRuleManager();
            if (ruleManager)
            {
                nx::vms::event::RulePtr rule = ruleManager->rule(params.id);
                if (rule)
                {
                    nx::vms::event::StringsHelper helper(commonModule()->systemContext());
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
        auto auditRecord = commonModule()->auditManager()->prepareRecord(authInfo, eventType);
        if (!description.isEmpty())
            auditRecord.addParam("description", description.toUtf8());
        if (!resourceId.isNull())
            auditRecord.resources.push_back(resourceId);
        commonModule()->auditManager()->addAuditRecord(auditRecord);
    }
}

void ECConnectionAuditManager::addAuditRecord(
    ApiCommand::Value command,
    const IdDataList& params,
    const QnAuthSession& authInfo)
{
    for (const IdData& param: params)
        addAuditRecord(command, param, authInfo);
}

void ECConnectionAuditManager::addAuditRecord(
    ApiCommand::Value /*command*/,
    const ResetEventRulesData& /*params*/,
    const QnAuthSession& authInfo)
{
    commonModule()->auditManager()->addAuditRecord(commonModule()->auditManager()->prepareRecord(authInfo, Qn::AR_BEventReset));
}

void ECConnectionAuditManager::addAuditRecord(
    ApiCommand::Value /*command*/,
    const DatabaseDumpData& /*params*/,
    const QnAuthSession& authInfo)
{
    commonModule()->auditManager()->addAuditRecord(
            commonModule()->auditManager()->prepareRecord(authInfo, Qn::AR_DatabaseRestore));
}

AbstractECConnection* ECConnectionAuditManager::ec2Connection() const
{
    return m_connection;
}

} // namespace ec2
