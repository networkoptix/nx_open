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
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/crypt/crypt.h>
#include <nx/vms/event/action_parameters.h>
#include <transaction/transaction_descriptor.h>

namespace ec2 {

extern const QString kHiddenPasswordFiller("******");

extern const std::set<QString> kResourceParamToAmend =
    []()
    {
        std::set<QString> s;
        s.insert(nx::vms::common::SystemSettings::Names::kWriteOnlyNames.begin(), nx::vms::common::SystemSettings::Names::kWriteOnlyNames.end());
        s.insert(ResourcePropertyKey::kWriteOnlyNames.begin(), ResourcePropertyKey::kWriteOnlyNames.end());
        return s;
    }();

bool amendOutputDataIfNeeded(
    const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::ResourceParamData* paramData)
{
    if (kResourceParamToAmend.contains(paramData->name))
    {
        if (accessData == Qn::kSystemAccess)
        {
            paramData->value = nx::crypt::decodeStringFromHexStringAES128CBC(paramData->value);
        }
        else if (paramData->name == ResourcePropertyKey::kCredentials
            || paramData->name == ResourcePropertyKey::kDefaultCredentials)
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

    if (accessData != Qn::kSystemAccess && paramData->name.startsWith("aes_key_"))
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
                nx::utils::log::Tag(QString("AmendOutputData")),
                "Failed to deserialize AesKey resource parameter value for '%1'",
                paramData->name);

            paramData->value.clear();
        }

        return true;
    }

    if (accessData != Qn::kSystemAccess && paramData->name == "ldap")
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
            NX_DEBUG(nx::utils::log::Tag(QString("AmendOutputData")),
                "Failed to deserialize LDAP settings %1", paramData->value);
            paramData->value.clear();
        }
        return true;
    }

    return false;
}

bool amendOutputDataIfNeeded(
    const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::StorageData* storageData)
{
    nx::utils::Url url(storageData->url);
    const nx::utils::log::Tag kLogTag(QString("AmendStorageData"));
    if (url.password().isEmpty())
    {
        NX_VERBOSE(kLogTag, "%1: Url '%2' password is empty", __func__, storageData->url);
        return false;
    }

    if (accessData == Qn::kSystemAccess || accessManager->hasPowerUserPermissions(accessData))
    {
        NX_VERBOSE(
            kLogTag, "%1: Decyphering url '%2' password",
            __func__, url.toDisplayString(QUrl::RemovePassword));
        url.setPassword(nx::crypt::decodeStringFromHexStringAES128CBC((url.password())));
    }
    else
    {
        NX_VERBOSE(
            kLogTag, "%1: Masking url '%2' password",
            __func__, url.toDisplayString(QUrl::RemovePassword));
        url.setPassword(kHiddenPasswordFiller);
    }

    storageData->url = url.toString();
    return true;
}

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::UserData* userData)
{
    if (accessData == Qn::kSystemAccess)
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

bool amendOutputDataIfNeeded(
    const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::MediaServerData* mediaServerData)
{
    if (accessData == Qn::kSystemAccess)
        return false;

    if (mediaServerData->authKey.isEmpty())
        return false;

    mediaServerData->authKey.clear();
    return true;
}

bool amendOutputDataIfNeeded(
    const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::MediaServerDataEx* mediaServerDataEx)
{
    bool result = amendOutputDataIfNeeded(
        accessData, accessManager, (nx::vms::api::MediaServerData*) mediaServerDataEx);
    for (auto& storage: mediaServerDataEx->storages)
        result |= amendOutputDataIfNeeded(accessData, accessManager, &storage);
    return result;
}

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::EventRuleData* rule)
{
    nx::vms::event::ActionParameters params;
    if (!QJson::deserialize<nx::vms::event::ActionParameters>(rule->actionParams, &params))
        return false;

    nx::utils::Url url = params.url;
    if (url.password().isEmpty())
        return false;

    const bool isGranted = accessData == Qn::kSystemAccess || accessManager->hasPowerUserPermissions(accessData);
    if (isGranted)
        url.setPassword(nx::crypt::decodeStringFromHexStringAES128CBC(url.password()));
    else
        url.setPassword(kHiddenPasswordFiller);
    params.url = url.toString();
    rule->actionParams = QJson::serialized(params);
    return true;
}

bool amendOutputDataIfNeeded(
    const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::ResourceParamWithRefData* paramData)
{
    return amendOutputDataIfNeeded(
        accessData, accessManager, static_cast<nx::vms::api::ResourceParamData*>(paramData));
}

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::FullInfoData* paramData)
{
    auto result = amendOutputDataIfNeeded(accessData, accessManager, &paramData->allProperties);
    result |= amendOutputDataIfNeeded(accessData, accessManager, &paramData->rules);
    result |= amendOutputDataIfNeeded(accessData, accessManager, &paramData->storages);
    result |= amendOutputDataIfNeeded(accessData, accessManager, &paramData->users);
    return result;
}

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::CameraDataEx* paramData)
{
    return amendOutputDataIfNeeded(accessData, accessManager, &paramData->addParams);
}

bool amendOutputDataIfNeeded(
    const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::ServerFootageData* paramData)
{
    if (accessData == Qn::kSystemAccess
        || accessManager->hasGlobalPermission(accessData, GlobalPermission::powerUser))
    {
        return false;
    }

    return nx::utils::erase_if(paramData->archivedCameras,
        [&](auto id)
        {
            auto resourcePool = accessManager->systemContext()->resourcePool();
            auto camera = resourcePool->getResourceById<QnVirtualCameraResource>(id);
            return !camera
                || !accessManager->hasPermission(accessData, camera, Qn::ViewFootagePermission);
        });
}

} // namespace ec2
