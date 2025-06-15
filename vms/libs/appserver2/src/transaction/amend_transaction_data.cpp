// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "amend_transaction_data.h"

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/fusion/serialization/json.h>
#include <nx/network/app_info.h>
#include <nx/utils/crypt/symmetrical.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/url.h>
#include <nx/vms/api/data/analytics_data.h>
#include <nx/vms/api/data/camera_data_ex.h>
#include <nx/vms/api/data/camera_history_data.h>
#include <nx/vms/api/data/credentials.h>
#include <nx/vms/api/data/discovery_data.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/api/data/full_info_data.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/api/data/ldap.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/resource_type_data.h>
#include <nx/vms/api/data/showreel_data.h>
#include <nx/vms/api/data/videowall_data.h>
#include <nx/vms/api/data/webpage_data.h>
#include <nx/vms/api/rules/rule.h>
#include <nx/vms/api/types/proxy_connection_access_policy.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/crypt/crypt.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/rules/encryption.h>
#include <transaction/transaction_descriptor.h>

namespace ec2 {

const std::set<QString>& resourceParamToAmend()
{
    static const auto kResult =
        []()
        {
            std::set<QString> s;
            s.insert(nx::vms::common::SystemSettings::Names::kWriteOnlyNames.begin(),
                nx::vms::common::SystemSettings::Names::kWriteOnlyNames.end());
            s.insert(ResourcePropertyKey::kWriteOnlyNames.begin(),
                ResourcePropertyKey::kWriteOnlyNames.end());
            return s;
        }();
    return kResult;
}

static const std::set<QString>& powerUsersOnlyResourceParamToAmend()
{
    static const std::set<QString> kResult{
        nx::vms::api::server_properties::kMetadataStorageIdKey,
        nx::vms::api::server_properties::kHddList};
    return kResult;
}

static bool amendOutputServerPortForwarding(
    const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::ResourceParamData* paramData)
{
    static const QString kEmptyPortForwarding = "[]";
    if (paramData->value == kEmptyPortForwarding)
        return false;
    const auto requiredPermissions =
        accessManager->globalSettings()->proxyConnectionAccessPolicy()
            == nx::vms::api::ProxyConnectionAccessPolicy::powerUsers
            ? nx::vms::api::GlobalPermission::powerUser
            : nx::vms::api::GlobalPermission::administrator;
    if (accessManager->hasGlobalPermission(accessData, requiredPermissions))
        return false;

    paramData->value = kEmptyPortForwarding;
    return true;
}

bool amendOutputDataIfNeeded(
    const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::ResourceParamData* paramData)
{
    if (resourceParamToAmend().contains(paramData->name))
    {
        if (accessData == nx::network::rest::kSystemAccess)
        {
            paramData->value = nx::crypt::decodeStringFromHexStringAES128CBC(paramData->value);
        }
        else if (paramData->name == nx::vms::api::device_properties::kCredentials
            || paramData->name == nx::vms::api::device_properties::kDefaultCredentials)
        {
            const bool hidePassword = !accessManager->globalSettings()->exposeDeviceCredentials()
                || !accessManager->hasPowerUserPermissions(accessData);
            const auto value = nx::crypt::decodeStringFromHexStringAES128CBC(paramData->value);
            paramData->value =
                nx::vms::api::Credentials::parseColon(value, hidePassword).asString();
        }
        else
        {
            paramData->value.clear();
        }
        return true;
    }
    if (accessData == nx::network::rest::kSystemAccess)
        return false;

    if (paramData->name.startsWith("aes_key_"))
    {
        using namespace nx::vms::crypt;
        bool success = false;
        auto key = QJson::deserialized<AesKeyWithTime>(paramData->value.toLatin1(), AesKey(), &success);
        if (success)
        {
            memset(key.cipherKey.data(), 0, sizeof(key.cipherKey));
            paramData->value = QJson::serialized(key);
        }
        else
        {
            NX_DEBUG(
                nx::log::Tag(QString("AmendOutputData")),
                "Failed to deserialize AesKey resource parameter value for '%1'",
                paramData->name);

            paramData->value.clear();
        }

        return true;
    }

    if (paramData->name == nx::vms::api::server_properties::kPortForwardingConfigurations)
        return amendOutputServerPortForwarding(accessData, accessManager, paramData);

    if (powerUsersOnlyResourceParamToAmend().contains(paramData->name))
    {
        if (paramData->value.isEmpty() || accessManager->hasPowerUserPermissions(accessData))
            return false;
        paramData->value.clear();
        return true;
    }

    if (paramData->name == "ldap")
    {
        bool success = false;
        auto ldap =
            QJson::deserialized(paramData->value.toUtf8(), nx::vms::api::LdapSettings(), &success);
        if (success)
        {
            ldap.adminPassword.reset();
            ldap.removeRecords = false;
            paramData->value = QJson::serialized(ldap);
        }
        else
        {
            NX_DEBUG(nx::log::Tag(QString("AmendOutputData")),
                "Failed to deserialize LDAP settings %1", paramData->value);
            paramData->value.clear();
        }
        return true;
    }

    return false;
}

bool amendOutputDataIfNeeded(
    const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::StorageData* storageData)
{
    nx::Url url(storageData->url);
    const nx::log::Tag kLogTag(QString("AmendStorageData"));
    if (url.password().isEmpty())
    {
        NX_VERBOSE(kLogTag, "%1: Url '%2' password is empty", __func__, storageData->url);
        return false;
    }

    if (accessData == nx::network::rest::kSystemAccess || accessManager->hasPowerUserPermissions(accessData))
    {
        NX_VERBOSE(
            kLogTag, "%1: Deciphering url '%2' password",
            __func__, url.toDisplayString(QUrl::RemovePassword));
        url.setPassword(nx::crypt::decodeStringFromHexStringAES128CBC((url.password())));
    }
    else
    {
        NX_VERBOSE(
            kLogTag, "%1: Masking url '%2' password",
            __func__, url.toDisplayString(QUrl::RemovePassword));
        url.setPassword(nx::Url::kMaskedPassword);
    }

    storageData->url = url.toString();
    return true;
}

bool amendOutputDataIfNeeded(const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::UserData* userData)
{
    if (accessData == nx::network::rest::kSystemAccess)
        return false;

    // All owners should be able to see all password data. This is required for merge with
    // taking remote settings.
    if (const auto accessUser = accessManager->systemContext()->resourcePool()
        ->getResourceById<QnUserResource>(accessData.userId); accessUser && accessUser->isAdministrator())
    {
        return false;
    }

    if (userData->id == QnUserResource::kAdminGuid)
    {
       // Only admin should see it's own password hashes!
       if (accessData.userId == userData->id)
           return false;
    }
    else if (accessManager->hasGlobalPermission(accessData, GlobalPermission::administrator))
    {
        return false;
    }
    else if (const auto user = accessManager->systemContext()->resourcePool()
             ->getResourceById<QnUserResource>(userData->id);
        accessManager->hasPermission(accessData, user, Qn::SavePermission))
    {
        return false;
    }

    // QnUserResource::digestAuthorizationEnabled() uses this value.
    if (userData->digest != nx::vms::api::UserData::kHttpIsDisabledStub)
        userData->digest.clear();
    userData->hash.clear();
    userData->cryptSha512Hash.clear();
    return true;
}

bool amendOutputDataIfNeeded(const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::UserGroupData* userGroupData)
{
    if (accessData == nx::network::rest::kSystemAccess
        || userGroupData->externalId.dn.isEmpty()
        || accessManager->hasPowerUserPermissions(accessData))
    {
        return false;
    }
    userGroupData->externalId.dn.clear();
    return true;
}

bool amendOutputDataIfNeeded(
    const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* /*accessManager*/,
    nx::vms::api::MediaServerData* mediaServerData)
{
    if (accessData == nx::network::rest::kSystemAccess)
        return false;

    if (mediaServerData->authKey.isEmpty())
        return false;

    mediaServerData->authKey.clear();
    return true;
}

bool amendOutputDataIfNeeded(
    const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::MediaServerDataEx* mediaServerDataEx)
{
    bool result = amendOutputDataIfNeeded(
        accessData, accessManager, (nx::vms::api::MediaServerData*) mediaServerDataEx);
    for (auto& storage: mediaServerDataEx->storages)
        result |= amendOutputDataIfNeeded(accessData, accessManager, &storage);

    if (accessData == nx::network::rest::kSystemAccess)
        return result;

    bool hasPowerUserPermissions = accessManager->hasPowerUserPermissions(accessData);
    for (auto& paramData: mediaServerDataEx->addParams)
    {
        if (paramData.name == nx::vms::api::server_properties::kPortForwardingConfigurations)
            result |= amendOutputServerPortForwarding(accessData, accessManager, &paramData);

        if (!hasPowerUserPermissions
            && powerUsersOnlyResourceParamToAmend().contains(paramData.name)
            && !paramData.value.isEmpty())
        {
            paramData.value.clear();
            result = true;
        }
    }
    return result;
}

bool amendOutputDataIfNeeded(
    const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::EventRuleData* rule)
{
    nx::vms::event::ActionParameters params;
    if (!QJson::deserialize<nx::vms::event::ActionParameters>(rule->actionParams, &params))
        return false;

    nx::Url url = params.url;
    if (url.password().isEmpty())
        return false;

    if (accessManager->hasPowerUserPermissions(accessData))
        url.setPassword(nx::crypt::decodeStringFromHexStringAES128CBC(url.password()));
    else
        url.setPassword(nx::Url::kMaskedPassword);
    params.url = url.toString();
    rule->actionParams = QJson::serialized(params);
    return true;
}

bool amendOutputDataIfNeeded(
    const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::rules::Rule* rule)
{
    int count = 0;

    for (auto& builder: rule->actionList)
    {
        for (auto& [_, field]: builder.fields)
        {
            for (const auto& name: nx::vms::rules::encryptedActionBuilderProperties(field.type))
            {
                if (auto it = field.props.find(name); it != field.props.end())
                {
                    const auto data = it.value().toString();
                    if (data.isEmpty())
                        continue;

                    if (accessManager->hasPowerUserPermissions(accessData))
                    {
                        it.value() =
                            nx::crypt::decodeStringFromHexStringAES128CBC(data);
                    }
                    else
                    {
                        it.value() = QString(nx::Url::kMaskedPassword);
                    }

                    ++count;
                }
            }
        }
    }

    return count > 0;
}

bool amendOutputDataIfNeeded(
    const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::ResourceParamWithRefData* paramData)
{
    return amendOutputDataIfNeeded(
        accessData, accessManager, static_cast<nx::vms::api::ResourceParamData*>(paramData));
}

bool amendOutputDataIfNeeded(const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::FullInfoData* paramData)
{
    auto result = amendOutputDataIfNeeded(accessData, accessManager, &paramData->allProperties);
    result |= amendOutputDataIfNeeded(accessData, accessManager, &paramData->rules);
    result |= amendOutputDataIfNeeded(accessData, accessManager, &paramData->vmsRules);
    result |= amendOutputDataIfNeeded(accessData, accessManager, &paramData->storages);
    result |= amendOutputDataIfNeeded(accessData, accessManager, &paramData->users);
    return result;
}

bool amendOutputDataIfNeeded(const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::CameraDataEx* paramData)
{
    return amendOutputDataIfNeeded(accessData, accessManager, &paramData->addParams);
}

bool amendOutputDataIfNeeded(
    const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::ServerFootageData* paramData)
{
    if (accessData == nx::network::rest::kSystemAccess
        || accessManager->hasGlobalPermission(accessData, GlobalPermission::powerUser))
    {
        return false;
    }

    return nx::utils::erase_if(paramData->archivedCameras,
        [&](auto id)
        {
            auto* resourcePool = accessManager->systemContext()->resourcePool();
            auto camera = resourcePool->getResourceById<QnVirtualCameraResource>(id);
            return !camera
                || !accessManager->hasPermission(accessData, camera, Qn::ViewFootagePermission);
        });
}

} // namespace ec2
