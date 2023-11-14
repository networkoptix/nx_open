// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "transaction_descriptor.h"

#include <iostream>

#include <QtCore/QCryptographicHash>

#include <common/common_module.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_access/user_access_data.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_property_key.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/data/api_fwd.h>
#include <nx/cloud/db/client/data/auth_data.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/license/usage_helper.h>

#include "amend_transaction_data.h"
#include "ec_connection_notification_manager.h"

using namespace nx::vms;

namespace ec2 {

namespace transaction_descriptor {

ErrorCode canModifyStorage(const CanModifyStorageData& data)
{
    if (data.modifyResourceResult != ErrorCode::ok)
        return data.modifyResourceResult;

    if (!data.hasExistingStorage)
        return ErrorCode::ok;

    const auto existingStorage = data.getExistingStorageDataFunc();
    if (existingStorage.parentId == data.request.parentId
        && data.request.url != existingStorage.url)
    {
        data.logErrorFunc(nx::format(
            "Got inconsistent update request for storage '%1'. Urls differ.", data.request.id));
        return ErrorCode::badRequest;
    }

    return ErrorCode::ok;
}

} // namespace transaction_descriptor

namespace detail {

namespace {

struct ServerApiErrors
{
    Q_DECLARE_TR_FUNCTIONS(ServerApiErrors);
};

} // namespace

struct InvalidGetHashHelper
{
    template<typename Param>
    QnUuid operator ()(const Param &)
    {
        // NX_ASSERT(0, "Invalid transaction for hash!");
        return QnUuid();
    }
};

struct InvalidTriggerNotificationHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &, const NotificationParams &)
    {
        NX_ASSERT(0, "This is a system transaction!"); // we MUSTN'T be here
    }
};

struct CreateHashByIdHelper
{
    template<typename Param>
    QnUuid operator ()(const Param &param) { return param.id; }
};

struct HardwareIdMappingHashHelper
{
    QnUuid operator()(const nx::vms::api::HardwareIdMapping& params)
    {
        return QnAbstractTransaction::makeHash(
            params.physicalIdGuid.toRfc4122(), "hardwareid_mapping");
    }
};

template<typename ClassType, typename FieldType>
struct CreateHashFromCustomField
{
    using MemberPointer = FieldType ClassType::*;

    MemberPointer memberPointer;

    CreateHashFromCustomField(MemberPointer memberPointer):
        memberPointer(memberPointer) {}

    QnUuid operator ()(const ClassType& param)
    {
        return guidFromArbitraryData(param.*memberPointer);
    }
};

template<typename ClassType, typename FieldType>
CreateHashFromCustomField<ClassType, FieldType> makeCreateHashFromCustomFieldHelper(
    FieldType ClassType::*memberPointer)
{
    return CreateHashFromCustomField<ClassType, FieldType>(memberPointer);
}

struct CreateHashByUserIdHelper
{
    template<typename Param>
    QnUuid operator ()(const Param &param) { return param.userId; }
};

struct CreateHashByIdRfc4122Helper
{
    CreateHashByIdRfc4122Helper(const QByteArray additionalData = QByteArray()) :
        m_additionalData(additionalData)
    {}

    template<typename Param>
    QnUuid operator ()(const Param &param) \
    {
        if (m_additionalData.isNull())
            return QnAbstractTransaction::makeHash(param.id.toRfc4122());
        return QnAbstractTransaction::makeHash(param.id.toRfc4122(), m_additionalData);
    }

private:
    QByteArray m_additionalData;
};

struct CreateHashForResourceParamWithRefDataHelper
{
    QnUuid operator ()(const nx::vms::api::ResourceParamWithRefData& param)
    {
        QCryptographicHash hash(QCryptographicHash::Md5);
        hash.addData("res_params");
        hash.addData(param.resourceId.toRfc4122());
        hash.addData(param.name.toUtf8());
        return QnUuid::fromRfc4122(hash.result());
    }
};

void apiIdDataTriggerNotificationHelper(
    const QnTransaction<nx::vms::api::IdData>& tran,
    const NotificationParams& notificationParams)
{
    switch (tran.command)
    {
        case ApiCommand::removeServerUserAttributes:
            return notificationParams.mediaServerNotificationManager->triggerNotification(
                tran,
                notificationParams.source);
        case ApiCommand::removeResource:
        case ApiCommand::removeResourceStatus:
            return notificationParams.resourceNotificationManager->triggerNotification(
                tran,
                notificationParams.source);
        case ApiCommand::removeCamera:
            return notificationParams.cameraNotificationManager->triggerNotification(
                tran,
                notificationParams.source);
        case ApiCommand::removeMediaServer:
        case ApiCommand::removeStorage:
            return notificationParams.mediaServerNotificationManager->triggerNotification(
                tran,
                notificationParams.source);
        case ApiCommand::removeUser:
        case ApiCommand::removeUserRole:
            return notificationParams.userNotificationManager->triggerNotification(
                tran,
                notificationParams.source);
        case ApiCommand::broadcastPeerSyncTime:
            return notificationParams.timeNotificationManager->triggerNotification(
                tran,
                notificationParams.source);
        case ApiCommand::removeEventRule:
            return notificationParams.businessEventNotificationManager->triggerNotification(
                tran,
                notificationParams.source);
        case ApiCommand::removeLayout:
            return notificationParams.layoutNotificationManager->triggerNotification(
                tran,
                notificationParams.source);
        case ApiCommand::removeLayoutTour:
            return notificationParams.layoutTourNotificationManager->triggerNotification(
                tran,
                notificationParams.source);
        case ApiCommand::removeVideowall:
            return notificationParams.videowallNotificationManager->triggerNotification(
                tran,
                notificationParams.source);
        case ApiCommand::removeWebPage:
            return notificationParams.webPageNotificationManager->triggerNotification(
                tran,
                notificationParams.source);
        case ApiCommand::removeCameraUserAttributes:
            return notificationParams.cameraNotificationManager->triggerNotification(
                tran,
                notificationParams.source);
        case ApiCommand::removeAccessRights:
            //#akolesnikov no notification needed
            break;
        case ApiCommand::removeAnalyticsPlugin:
        case ApiCommand::removeAnalyticsEngine:
            return notificationParams.analyticsNotificationManager->triggerNotification(
                tran,
                notificationParams.source);
            break;
        default:
            NX_ASSERT(false);
    }
}

QnUuid createHashForServerFootageDataHelper(const nx::vms::api::ServerFootageData& params)
{
    return QnAbstractTransaction::makeHash(params.serverGuid.toRfc4122(), "history");
}

QnUuid createHashForApiCameraAttributesDataHelper(const nx::vms::api::CameraAttributesData& params)
{
    return QnAbstractTransaction::makeHash(params.cameraId.toRfc4122(), "camera_attributes");
}

QnUuid createHashForApiAccessRightsDataHelper(const nx::vms::api::AccessRightsData& params)
{
    return QnAbstractTransaction::makeHash(params.userId.toRfc4122(), "access_rights");
}

QnUuid createHashForApiLicenseDataHelper(const nx::vms::api::LicenseData& params)
{
    return QnAbstractTransaction::makeHash(params.key, "ApiLicense");
}

QnUuid createHashForApiMediaServerUserAttributesDataHelper(
    const api::MediaServerUserAttributesData &params)
{
    return QnAbstractTransaction::makeHash(params.serverId.toRfc4122(), "server_attributes");
}

QnUuid createHashForApiStoredFileDataHelper(const nx::vms::api::StoredFileData& params)
{
    return QnAbstractTransaction::makeHash(params.path.toUtf8());
}

QnUuid createHashForApiDiscoveryDataHelper(const nx::vms::api::DiscoveryData& params)
{
    return QnAbstractTransaction::makeHash("discovery_data", params);
}

struct CameraNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.cameraNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct ResourceNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.resourceNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct MediaServerNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.mediaServerNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct UserNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.userNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct TimeNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.timeNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct LayoutNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.layoutNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct LayoutTourNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.layoutTourNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct VideowallNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.videowallNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct BusinessEventNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.businessEventNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct VmsRulesNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.vmsRulesNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct StoredFileNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.storedFileNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct LicenseNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.licenseNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct DiscoveryNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.discoveryNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct WebPageNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.webPageNotificationManager->triggerNotification(tran, notificationParams.source);
    }
};

struct AnalyticsNotificationManagerHelper
{
    template<typename Param>
    void operator()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.analyticsNotificationManager
            ->triggerNotification(tran, notificationParams.source);
    }
};

struct EmptyNotificationHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &, const NotificationParams &) {}
};

void apiIdDataListTriggerNotificationHelper(
    const QnTransaction<nx::vms::api::IdDataList>& tran,
    const NotificationParams& notificationParams)
{
    switch (tran.command)
    {
        case ApiCommand::removeStorages:
            return notificationParams.mediaServerNotificationManager->triggerNotification(
                tran,
                notificationParams.source);
        case ApiCommand::removeResources:
            return notificationParams.resourceNotificationManager->triggerNotification(
                tran,
                notificationParams.source);
        default:
            NX_ASSERT(false);
    }
}

// Permission helpers
struct InvalidAccess
{
    template<typename Param>
    Result operator()(QnCommonModule*, const Qn::UserAccessData&, const Param&)
    {
        static const QString kErrorMessage = NX_FMT("Invalid access check for %1", typeid(Param));
        NX_ASSERT(false, kErrorMessage);
        return Result(ErrorCode::forbidden, QString(kErrorMessage));
    }
};

struct InvalidAccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(QnCommonModule* commonModule,
        const Qn::UserAccessData&, const Param&)
    {
        NX_ASSERT(false, "Invalid outgoing transaction access check (%1). We shouldn't be here.",
            typeid(Param).name());
        return RemotePeerAccess::Forbidden;
    }
};

bool hasSystemAccess(const Qn::UserAccessData& accessData)
{
    return accessData.access == Qn::UserAccessData::Access::System;
}

struct SystemSuperUserAccessOnly
{
    template<typename Param>
    bool operator()(QnCommonModule*, const Qn::UserAccessData& accessData, const Param&)
    {
        return hasSystemAccess(accessData);
    }
};

struct SystemSuperUserAccessOnlyOut
{
    template<typename Param>
    RemotePeerAccess operator()(QnCommonModule*, const Qn::UserAccessData& accessData, const Param&)
    {
        return hasSystemAccess(accessData)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct AllowForAllAccess
{
    template<typename Param>
    Result operator()(QnCommonModule*, const Qn::UserAccessData&, const Param&)
    {
        return Result();
    }
};

struct AllowForAllAccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(QnCommonModule*, const Qn::UserAccessData&, const Param&) { return RemotePeerAccess::Allowed; }
};

static Result checkExistingResourceAccess(
    QnCommonModule* commonModule,
    const Qn::UserAccessData& accessData,
    const QnUuid& resourceId,
    Qn::Permissions permissions)
{
    const auto& resPool = commonModule->resourcePool();
    auto userResource = resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
    // Null resource Id can not be handled by permissions engine, since there is no such resource.
    // System settings are stored as admin user properties
    if ((resourceId.isNull() || resourceId == QnUserResource::kAdminGuid)
        && userResource
        && (userResource->userRole() == Qn::UserRole::owner
            || userResource->userRole() == Qn::UserRole::administrator))
    {
        return Result();
    }

    QnResourcePtr target = resPool->getResourceById(resourceId);
    if (commonModule->resourceAccessManager()->hasPermission(
        userResource, target, permissions))
    {
        return Result();
    }

    const QString errorMessage = NX_FMT(
        "User %1 with %2 permissions is asking for %3 resource with %4 permissions and fails",
        userResource ? userResource->getName() : accessData.userId.toString(),
        accessData.access,
        resourceId,
        permissions);
    return Result(ErrorCode::forbidden, errorMessage);
}

static Result checkReadResourceAccess(
    QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const QnUuid& resourceId)
{
    if (hasSystemAccess(accessData))
        return Result();

    if (accessData.access == Qn::UserAccessData::Access::ReadAllResources)
        return Result();

    return checkExistingResourceAccess(commonModule, accessData, resourceId, Qn::ReadPermission);
}

static Result checkSaveResourceAccess(
    QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const QnUuid& resourceId)
{
    if (hasSystemAccess(accessData))
        return Result();

    return checkExistingResourceAccess(commonModule, accessData, resourceId, Qn::SavePermission);
}

static Result checkReadResourceParamAccess(
    QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const QnUuid& resourceId)
{
    return checkReadResourceAccess(commonModule, accessData, resourceId);
}

struct ModifyResourceAccess
{
    ModifyResourceAccess() {}

    template<typename Param>
    Result operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData,
        const Param& param)
    {
        NX_VERBOSE(this,
            "Got modify resource request. Is system access: %1, Data type: %2, Data contents: %3",
            hasSystemAccess(accessData), typeid(param), QJson::serialized(param));

        if (hasSystemAccess(accessData))
            return Result();

        const auto& resPool = commonModule->resourcePool();
        auto userResource = resPool->getResourceById(accessData.userId)
            .dynamicCast<QnUserResource>();
        QnResourcePtr target = resPool->getResourceById(param.id);

        bool result = false;
        if (!target)
            result = commonModule->resourceAccessManager()->canCreateResourceFromData(userResource, param);
        else
            result = commonModule->resourceAccessManager()->canModifyResource(userResource, target, param);

        if (!result)
        {
            QString errorMessage = target
                ? NX_FMT(
                    "User %1 is not permitted to modify %2",
                    userResource ? userResource->getId().toSimpleString() : QString(),
                    target->getId().toSimpleString())
                : NX_FMT(
                    "User %1 is not permitted to create new resource",
                    userResource ? userResource->getId().toSimpleString() : QString());
            return Result(ErrorCode::forbidden, std::move(errorMessage));
        }

        auto typeDescriptor = qnResTypePool->getResourceType(param.typeId);
        if (!typeDescriptor)
        {
            QString errorMessage = NX_FMT("Invalid resource type %1", param.typeId);
            return Result(ErrorCode::badRequest, std::move(errorMessage));
        }

        return Result();
    }
};

struct ModifyStorageAccess
{
    Result operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::StorageData& param)
    {
        if (param.url.isEmpty())
        {
            NX_DEBUG(this, "Declining save storage request because provided url is empty");
            return Result(ErrorCode::badRequest, "Empty url is not allowed");
        }

        transaction_descriptor::CanModifyStorageData data;
        const auto existingResource = commonModule->resourcePool()->getResourceById(param.id);
        data.hasExistingStorage = (bool) existingResource;
        data.getExistingStorageDataFunc =
            [&]()
            {
                nx::vms::api::ResourceData result;
                fromResourceToApi(existingResource, result);
                return result;
            };

        data.logErrorFunc = [this](const QString& message) { NX_DEBUG(this, message); };
        data.modifyResourceResult = ModifyResourceAccess()(commonModule, accessData, param);
        data.request = param;
        amendOutputDataIfNeeded(accessData, commonModule->resourceAccessManager(), &data.request);

        return transaction_descriptor::canModifyStorage(data);
    }
};

struct RemoveResourceAccess
{
    RemoveResourceAccess()  {}

    template<typename Param>
    Result operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData,
        const Param& param)
    {
        NX_VERBOSE(this,
            "Got remove resource request. Is system access: %1, Data type: %2, Data contents: %3",
            hasSystemAccess(accessData), typeid(param), QJson::serialized(param));

        if (hasSystemAccess(accessData))
            return Result();

        const auto& resPool = commonModule->resourcePool();
        auto userResource = resPool->getResourceById(accessData.userId)
            .dynamicCast<QnUserResource>();
        QnResourcePtr target = resPool->getResourceById(param.id);
        if (!commonModule->resourceAccessManager()->hasPermission(
            userResource, target, Qn::RemovePermission))
        {
            QString errorMessage = NX_FMT(
                "User %1 is not permitted to remove %2",
                userResource ? userResource->getId().toSimpleString() : QString(),
                target ? target->getId().toSimpleString() : QString());
            return Result(ErrorCode::forbidden, std::move(errorMessage));
        }

        const auto storage = target.dynamicCast<QnStorageResource>();
        bool isOwnStorage = (storage && storage->getParentId() == commonModule->peerId());
        if (isOwnStorage && !storage->isExternal() && storage->isWritable())
        {
            NX_DEBUG(this, "Attempt to delete own local storage %1", storage->getId());
            return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
                "It is forbidden to delete own local storage %1."),
                storage->getId().toSimpleString()));
        }

        return Result();
    }
};

struct SaveUserAccess
{
    Result operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData,
        const nx::vms::api::UserData& param)
    {
        if (hasSystemAccess(accessData))
            return ModifyResourceAccess()(commonModule, accessData, param);

        const bool hasUserWithSameName = commonModule->resourcePool()->contains<QnUserResource>(
            [name = param.name.toLower(), id = param.id](const auto& u)
            {
                return u->getName().toLower() == name && u->getId() != id;
            });
        if (hasUserWithSameName)
        {
            QString errorMessage =
                NX_FMT("Won't save user '%1' because of the name duplication", param.name);
            return Result(ErrorCode::forbidden, std::move(errorMessage));
        }

        const auto existingUser =
            commonModule->resourcePool()->getResourceById<QnUserResource>(param.id);

        // Won't allow to change the user name without providing the password because it's not
        // possible to recalculate hashes.
        if (existingUser
            && param.digest != nx::vms::api::UserData::kHttpIsDisabledStub
            && (param.isCloud || existingUser->getDigest() == param.digest)
            && existingUser->getName() != param.name)
        {
            QString errorMessage = NX_FMT(
                "Won't save existing user '%1' because names differ: '%1' vs '%2' and %3.",
                existingUser->getName(), param.name,
                param.isCloud
                    ? "changing name is forbidden for cloud users"
                    : "password has not been provided");

            return Result(ErrorCode::forbidden, std::move(errorMessage));
        }

        if (!existingUser && param.name.isEmpty())
            return Result(ErrorCode::badRequest, "Won't save new user with empty name.");

        if (!existingUser && param.isAdmin)
            return Result(ErrorCode::forbidden, "Creating an owner user is not allowed.");

        if (!commonModule->userRolesManager()->hasRoles(param.userRoleIds))
            return Result(ErrorCode::badRequest, "User Role does not exist.");

        if (param.isCloud
            && ((existingUser && param.fullName != existingUser->fullName())
                || (!existingUser && !param.fullName.isEmpty())))
        {
            return Result(ErrorCode::forbidden, "Cloud user full name is controlled by the Cloud");
        }

        auto r = ModifyResourceAccess()(commonModule, accessData, param);
        return r;
    }
};

struct ModifyCameraDataAccess
{
    Result operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::CameraData& param)
    {
        if (!hasSystemAccess(accessData))
        {
            if (!param.physicalId.isEmpty() && !param.id.isNull())
            {
                const auto expectedId = nx::vms::api::CameraData::physicalIdToId(param.physicalId);
                if (expectedId != param.id)
                {
                    QString errorMessage = NX_FMT(
                        "Expected camera id %1 != actual %2 (physicalId %3)",
                        expectedId, param.id, param.physicalId);
                    return Result(ErrorCode::badRequest, std::move(errorMessage));
                }
            }
        }

        return ModifyResourceAccess()(commonModule, accessData, param);
    }
};

template<typename Param>
void applyColumnFilter(QnCommonModule*, const Qn::UserAccessData& /*accessData*/, Param& /*data*/)
{
}

void applyColumnFilter(
    QnCommonModule*, const Qn::UserAccessData& accessData, api::MediaServerData& data)
{
    if (accessData != Qn::kSystemAccess)
        data.authKey.clear();
}

void applyColumnFilter(QnCommonModule* commonModule,
    const Qn::UserAccessData& accessData,
    api::MediaServerDataEx& data)
{
    if (accessData != Qn::kSystemAccess)
        data.authKey.clear();
}

void applyColumnFilter(
    QnCommonModule* commonModule, const Qn::UserAccessData& accessData, api::StorageData& data)
{
    if (!hasSystemAccess(accessData)
        && !commonModule->resourceAccessManager()->hasGlobalPermission(
            accessData, GlobalPermission::admin))
    {
        data.url = QnStorageResource::urlWithoutCredentials(data.url);
    }
}

struct ReadResourceAccess
{
    template<typename Param>
    Result operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, Param& param)
    {
        if (const auto r = checkReadResourceAccess(commonModule, accessData, param.id); !r)
            return r;

        applyColumnFilter(commonModule, accessData, param);
        return Result();
    }
};

struct ReadResourceAccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const Param& param)
    {
        return checkReadResourceAccess(commonModule, accessData, param.id)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ReadResourceParamAccess
{
    Result operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        nx::vms::api::ResourceParamWithRefData& param)
    {
        if (const auto r =
            checkReadResourceParamAccess(commonModule, accessData, param.resourceId)
            ;
            !r)
        {
            return r;
        }
        operator()(
            commonModule,
            accessData,
            static_cast<nx::vms::api::ResourceParamData&>(param));
        return Result();
    }

    Result operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        nx::vms::api::ResourceParamData& param)
    {
        const auto accessManager = commonModule->resourceAccessManager();
        if (accessData == Qn::kSystemAccess
            || accessData.access == Qn::UserAccessData::Access::ReadAllResources
            || accessManager->hasGlobalPermission(accessData, GlobalPermission::admin))
        {
            return Result();
        }

        QString errorMessage = NX_FMT(
            "User %1 with %2 permissions is asking for reading resource parameter %3 and fails",
            accessData.userId,
            accessData.access,
            param.name);
        return Result(ErrorCode::forbidden, std::move(errorMessage));
    }
};

struct ReadResourceParamAccessOut
{
    RemotePeerAccess operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::ResourceParamWithRefData& param)
    {
        return checkReadResourceParamAccess(commonModule, accessData, param.resourceId)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ModifyResourceParamAccess
{
    static const std::set<QString> kSystemAccessOnlyProperties;

    static QString userNameOrId(const QnUserResourcePtr& user, const Qn::UserAccessData& accessData)
    {
        return user ? user->getName() : accessData.userId.toString();
    }

    static QString userNameOrId(const Qn::UserAccessData& accessData, QnCommonModule* commonModule)
    {
        auto user = commonModule->resourcePool()->getResourceById<QnUserResource>(accessData.userId);
        return userNameOrId(user, accessData);
    }

    static bool hasSameProperty(
        const QnResourcePtr& target, const nx::vms::api::ResourceParamWithRefData& param)
    {
        auto value = target->getProperty(param.name);
        return !value.isNull() && value == param.value;
    }

    static bool hasSameProperty(
        QnCommonModule* commonModule, const nx::vms::api::ResourceParamWithRefData& param)
    {
        auto target = commonModule->resourcePool()->getResourceById(param.resourceId);
        return target && hasSameProperty(target, param);
    }

    ModifyResourceParamAccess(bool isRemove): isRemove(isRemove) {}

    Result operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::ResourceParamWithRefData& param)
    {
        if (hasSystemAccess(accessData))
            return Result();

        const bool isNewApiCompoundTransaction =
            param.checkResourceExists != nx::vms::api::CheckResourceExists::yes;

        if (!isRemove
            && isNewApiCompoundTransaction
            && hasSameProperty(commonModule, param))
        {
            // CRUD API PATCH merges with existing values represented as JSON object so some of
            // them are not changed.
            return Result();
        }

        if (kSystemAccessOnlyProperties.find(param.name) != kSystemAccessOnlyProperties.cend())
        {
            return Result(ErrorCode::forbidden, NX_FMT(
                "User '%1' with %2 permissions can not modify resource parameter '%3'",
                userNameOrId(accessData, commonModule), accessData.access, param.name));
        }

        const auto& resPool = commonModule->resourcePool();
        auto userResource =
            resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();

        // System properties are stored in resource unrelated places and should be handled
        // differently.
        if (userResource
            && (userResource->userRole() == Qn::UserRole::owner
                || userResource->userRole() == Qn::UserRole::administrator))
        {
            // Null resource Id can not be handled by permissions engine, since there is no such resource.
            if (param.resourceId.isNull())
                return Result();

            // System settings are stored as admin user properties. Admin user is an owner and can not
            // be modified by anyone else.
            // TODO: Remove this check once global system settings are not admin properties.
            if (param.resourceId == QnUserResource::kAdminGuid)
                return Result();
        }

        auto accessManager = commonModule->resourceAccessManager();
        auto target = resPool->getResourceById(param.resourceId);
        if (!isRemove && param.name == ResourcePropertyKey::Server::kMetadataStorageIdKey)
        {
            const auto metadataStorageId = QnUuid::fromStringSafe(param.value);
            const auto server = resPool->getResourceById<QnMediaServerResource>(param.resourceId);
            if (!NX_ASSERT(server))
            {
                return Result(ErrorCode::serverError,
                    NX_FMT("Server %1 does not exist", param.resourceId));
            }

            const auto storages = server->getStorages();
            const auto storage = nx::utils::find_if(storages,
                [&metadataStorageId](const auto& s) { return s->getId() == metadataStorageId; });

            if (!storage)
            {
                return Result(ErrorCode::badRequest,
                    NX_FMT("Storage %1 does not belong to the server", param.value));
            }
        }

        if (isNewApiCompoundTransaction)
        {
            if (accessManager->hasGlobalPermission(userResource, GlobalPermission::admin))
                return Result();
        }

        Qn::Permissions permissions = Qn::SavePermission;
        if (param.name == Qn::USER_FULL_NAME)
            permissions |= Qn::WriteFullNamePermission;

        return accessManager->hasPermission(userResource, target, permissions)
            ? Result()
            : Result(ErrorCode::forbidden, NX_FMT(
                "User '%1' with %2 permissions can not modify resource parameter of %3",
                userNameOrId(userResource, accessData), accessData.access, param.resourceId));
    }

    bool isRemove;
};

const std::set<QString> ModifyResourceParamAccess::kSystemAccessOnlyProperties = {
    ResourcePropertyKey::kForcedLicenseType
};

struct ReadFootageDataAccess
{
    Result operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::ServerFootageData& param)
    {
        return checkReadResourceAccess(commonModule, accessData, param.serverGuid);
    }
};

struct ReadFootageDataAccessOut
{
    RemotePeerAccess operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::ServerFootageData& param)
    {
        return checkReadResourceAccess(commonModule, accessData, param.serverGuid)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ModifyFootageDataAccess
{
    Result operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::ServerFootageData& param)
    {
        return checkSaveResourceAccess(commonModule, accessData, param.serverGuid);
    }
};

struct ReadCameraAttributesAccess
{
    Result operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::CameraAttributesData& param)
    {
        return checkReadResourceAccess(commonModule, accessData, param.cameraId);
    }
};

struct ReadCameraAttributesAccessOut
{
    RemotePeerAccess operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::CameraAttributesData& param)
    {
        return checkReadResourceAccess(commonModule, accessData, param.cameraId)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ModifyCameraAttributesAccess
{
    Result operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::CameraAttributesData& param)
    {
        if (hasSystemAccess(accessData))
            return Result();

        const auto& resPool = commonModule->resourcePool();
        auto accessManager = commonModule->resourceAccessManager();
        auto camera = resPool->getResourceById<QnVirtualCameraResource>(param.cameraId);
        if (camera)
        {
            // CRUD API PATCH merges with existing attributes represented as JSON object so they
            // can be not changed.
            nx::vms::api::CameraAttributesData origin = camera->getUserAttributes();
            if (origin == param)
                return Result();
        }
        else
        {
            if (param.scheduleEnabled)
            {
                return Result(
                    ErrorCode::forbidden, "Device creation with scheduleEnabled is forbidden.");
            }

            if (param.checkResourceExists != nx::vms::api::CheckResourceExists::yes)
            {
                if (accessManager->hasGlobalPermission(accessData, GlobalPermission::admin))
                    return Result();
            }
        }

        auto user = resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
        if (!accessManager->hasPermission(user, camera, Qn::SavePermission))
        {
            return Result(ErrorCode::forbidden,
                "Saving Device attributes is forbidden: no saving permission.");
        }

        using namespace nx::vms::license;
        CamLicenseUsageHelper licenseUsageHelper(commonModule->systemContext());

        if (!param.motionMask.isEmpty() && !parseMotionRegionList(param.motionMask))
        {
            return Result(ErrorCode::badRequest,
                nx::format(ServerApiErrors::tr("Motion mask should be in range: x in [0, %1], y in [0, %2], width in [1, %3], height in [1, %4]"),
                    Qn::kMotionGridWidth - 1,
                    Qn::kMotionGridHeight - 1,
                    Qn::kMotionGridWidth,
                    Qn::kMotionGridHeight));
        }

        // Check the license if and only if recording goes from 'off' to 'on' state
        if (param.scheduleEnabled && !camera->isScheduleEnabled())
        {
            using namespace nx::vms::license;
            CamLicenseUsageHelper licenseUsageHelper(commonModule->systemContext());
            QnVirtualCameraResourceList cameras;

            licenseUsageHelper.propose(camera, param.scheduleEnabled);
            if (licenseUsageHelper.isOverflowForCamera(camera))
            {
                QString errorMessage = NX_FMT(
                    "Saving CameraAttributesData %1 is forbidden: no license to enable recording.",
                    param.cameraId);
                return Result(ErrorCode::forbidden, std::move(errorMessage));
            }
        }

        return Result();
    }
};

struct ModifyCameraAttributesListAccess
{
    void operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        nx::vms::api::CameraAttributesDataList& param)
    {
        if (hasSystemAccess(accessData))
            return;

        for (const auto& p: param)
        {
            if (!checkSaveResourceAccess(commonModule, accessData, p.cameraId))
            {
                param = {};
                return;
            }
        }

        using namespace nx::vms::license;
        CamLicenseUsageHelper licenseUsageHelper(commonModule->systemContext());
        QnVirtualCameraResourceList cameras;

        const auto& resPool = commonModule->resourcePool();
        for (const auto& p: param)
        {
            auto camera = resPool->getResourceById(p.cameraId).dynamicCast<QnVirtualCameraResource
            >();
            if (!camera)
            {
                param = {};
                return;
            }
            cameras.push_back(camera);
            const bool prevScheduleEnabled = camera->isScheduleEnabled();
            if (prevScheduleEnabled != p.scheduleEnabled)
                licenseUsageHelper.propose(camera, p.scheduleEnabled);
        }

        for (const auto& camera: cameras)
            if (licenseUsageHelper.isOverflowForCamera(camera))
            {
                param = {};
                return;
            }
    }
};

struct ReadServerAttributesAccess
{
    Result operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const api::MediaServerUserAttributesData& param)
    {
        return checkReadResourceAccess(commonModule, accessData, param.serverId);
    }
};

struct ReadServerAttributesAccessOut
{
    RemotePeerAccess operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData,
        const api::MediaServerUserAttributesData& param)
    {
        return checkReadResourceAccess(commonModule, accessData, param.serverId)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ModifyServerAttributesAccess
{
    Result operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const api::MediaServerUserAttributesData& param)
    {
        if (hasSystemAccess(accessData))
            return Result();

        const auto& resPool = commonModule->resourcePool();
        auto accessManager = commonModule->resourceAccessManager();
        auto server = resPool->getResourceById<QnMediaServerResource>(param.serverId);
        if (server)
        {
            // CRUD API PATCH merges with existing attributes represented as JSON object so they
            // can be not changed.
            if (server->userAttributes() == param)
                return Result();
        }
        else if (param.checkResourceExists != nx::vms::api::CheckResourceExists::yes)
        {
            if (accessManager->hasGlobalPermission(accessData, GlobalPermission::admin))
                return Result();
        }

        auto user = resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
        return accessManager->hasPermission(user, server, Qn::SavePermission)
            ? Result()
            : Result(ErrorCode::forbidden,
                "Saving Server attributes is forbidden: no saving permission.");
    }
};

static Result userAccessHelper(
    QnCommonModule* commonModule,
    const Qn::UserAccessData& accessData,
    GlobalPermission permissions)
{
    if (hasSystemAccess(accessData))
        return Result();
    const auto& resPool = commonModule->resourcePool();
    auto userResource = resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
    if (!commonModule->resourceAccessManager()->hasGlobalPermission(userResource, permissions))
    {
        QString errorMessage = NX_FMT(
            "User %1 with %2 permissions is asking for %3 permissions and fails",
            userResource ? userResource->getName() : accessData.userId.toString(),
            accessData.access,
            permissions);
        return Result(ErrorCode::forbidden, std::move(errorMessage));
    }

    return Result();
}

struct UserInputAccess
{
    template<typename Param>
    Result operator()(
        QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const Param&)
    {
        return userAccessHelper(commonModule, accessData, GlobalPermission::userInput);
    }
};

struct AdminOnlyAccess
{
    template<typename Param>
    Result operator()(
        QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const Param&)
    {
        return userAccessHelper(commonModule, accessData, GlobalPermission::admin);
    }
};

struct AdminOnlyAccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(
        QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const Param&)
    {
        return userAccessHelper(commonModule, accessData, GlobalPermission::admin)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct SaveUserRoleAccess
{
    Result operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::UserRoleData& param)
    {
        if (auto r = AdminOnlyAccess()(commonModule, accessData, param); !r)
        {
            r.message = NX_FMT("Saving user role is forbidden because the user has no admin access");
            return r;
        }

        const auto parentRoles = commonModule->userRolesManager()->userRoles(param.parentRoleIds);
        if (const auto cycledRole =
            nx::utils::find_if(parentRoles, [&](const auto& role) { return role.id == param.id; }))
        {
            return Result(ErrorCode::forbidden, NX_FMT(
                "Parent role cycle is forbidden: This role is already inherided by: %1",
                cycledRole->name));
        }

        return Result();
    }
};

struct RemoveUserRoleAccess
{
    Result operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::IdData& param)
    {
        if (auto r = AdminOnlyAccess()(commonModule, accessData, param); !r)
        {
            r.message = NX_FMT("Removing user role is forbidden because the user has no admin access");
            return r;
        }

        for (const auto& user: commonModule->resourcePool()->getResources<QnUserResource>())
        {
            if (nx::utils::find_if(user->userRoleIds(), [&](const auto& id) { return id == param.id; }))
            {
                QString errorMessage = NX_FMT(
                    "Removing user role is forbidden because the role is still used by the user: %1",
                    user->getName());
                return Result(ErrorCode::forbidden, std::move(errorMessage));
            }
        }

        for (const auto& role: commonModule->userRolesManager()->userRoles())
        {
            if (nx::utils::find_if(role.parentRoleIds, [&](const auto& id) { return id == param.id; }))
            {
                QString errorMessage = NX_FMT(
                    "Removing user role is forbidden because the role is still inherited by the role: %1",
                    role.name);
                return Result(ErrorCode::forbidden, std::move(errorMessage));
            }
        }

        return Result();
    }
};

struct ModifyAccessRightsChecker
{
    Result operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::AccessRightsData& param)
    {
        if (hasSystemAccess(accessData))
            return Result();

        auto user = commonModule->resourcePool()->getResourceById<QnUserResource>(param.userId);
        if (!param.resourceIds.empty())
        {
            if ((param.checkResourceExists == nx::vms::api::CheckResourceExists::customRole)
                || ((param.checkResourceExists == nx::vms::api::CheckResourceExists::yes) && user
                    && (user->userRole() == Qn::UserRole::customUserRole)))
            {
                return Result(ErrorCode::forbidden,
                    "User with a custom user role is not allowed to change shared Resources.");
            }
        }

        auto accessManager = commonModule->resourceAccessManager();

        // CRUD API PATCH merges with existing shared resources so they can be not changed.
        QSet<QnUuid> sharedResources;
        if (user)
        {
            sharedResources = commonModule->sharedResourcesManager()->sharedResources(user);
        }
        else
        {
            auto role = commonModule->userRolesManager()->userRole(param.userId);
            if (role.id.isNull())
            {
                // We can clear shared Resources even after the User or Role is deleted.
                if (!param.resourceIds.empty()
                    && (param.checkResourceExists == nx::vms::api::CheckResourceExists::yes))
                {
                    return Result(ErrorCode::badRequest, NX_FMT(
                        "To set shared Resources, %1 must be a valid User or Role", param.userId));
                }
            }
            else
            {
                sharedResources = commonModule->sharedResourcesManager()->sharedResources(role);
            }
        }
        if (sharedResources == QSet<QnUuid>(param.resourceIds.begin(), param.resourceIds.end()))
            return Result();

        return accessManager->hasGlobalPermission(accessData, GlobalPermission::admin)
            ? Result()
            : Result(ErrorCode::forbidden, "Admin permissions required.");
    }
};

struct VideoWallControlAccess
{
    Result operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::VideowallControlMessageData&)
    {
        if (auto r = userAccessHelper(commonModule, accessData, GlobalPermission::controlVideowall)
            ;
            !r)
        {
            r.message = NX_FMT(
                "Access check for ApiVideoWallControlMessageData failed for user %1",
                accessData.userId);
            return r;
        }
        return Result();
    }
};

struct LayoutTourAccess
{
    Result operator()(
        QnCommonModule* /*commonModule*/,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::LayoutTourData& tour)
    {
        if (hasSystemAccess(accessData)
            || tour.parentId.isNull()
            || accessData.userId == tour.parentId)
        {
            return Result();
        }
        QString errorMessage = NX_FMT(
            "User %1 is not allowed to modify layout tour with parentId %2",
            accessData.userId,
            tour.parentId);
        return Result(ErrorCode::forbidden, std::move(errorMessage));
    }
};

struct LayoutTourAccessById
{
    Result operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::IdData& tourId)
    {
        const auto tour = commonModule->layoutTourManager()->tour(tourId.id);
        if (!tour.isValid())
            return Result(); //< Allow everyone to work with tours which are already deleted.
        return LayoutTourAccess()(commonModule, accessData, tour);
    }
};

struct InvalidFilterFunc
{
    template<typename ParamType>
    void operator()(QnCommonModule*, const Qn::UserAccessData&, ParamType&)
    {
        NX_ASSERT(0, lit("This transaction (%1) param type doesn't support filtering").arg(typeid(ParamType).name()));
    }
};

template<typename SingleAccess>
struct AccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(
        QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const Param& param)
    {
        return SingleAccess()(commonModule, accessData, param)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

template<typename SingleAccess>
struct FilterListByAccess
{
    template<typename ParamContainer>
    void operator()(
        QnCommonModule* commonModule, const Qn::UserAccessData& accessData, ParamContainer& outList)
    {
        outList.erase(std::remove_if(
            outList.begin(),
            outList.end(),
            [&accessData, commonModule](auto& param) -> bool
            {
                return !SingleAccess()(commonModule, accessData, param);
            }),
            outList.end());
    }

    void operator()(
        QnCommonModule* commonModule, const Qn::UserAccessData& accessData, api::MediaServerDataExList& outList)
    {
        outList.erase(std::remove_if(
            outList.begin(),
            outList.end(),
            [&accessData, commonModule](const auto& param) -> bool
            {
                return !SingleAccess()(commonModule, accessData, param);
            }),
            outList.end());

        for (auto& i: outList)
            FilterListByAccess()(commonModule, accessData, i.storages);

    }
};

template<>
struct FilterListByAccess<ModifyResourceAccess>
{
    FilterListByAccess() {}

    template<typename ParamContainer>
    void operator()(
        QnCommonModule* commonModule, const Qn::UserAccessData& accessData, ParamContainer& outList)
    {
        outList.erase(std::remove_if(
            outList.begin(),
            outList.end(),
            [&accessData, commonModule](const auto& param) -> bool
            {
                return !ModifyResourceAccess()(commonModule, accessData, param);
            }),
            outList.end());
    }
};

template<>
struct FilterListByAccess<ModifyResourceParamAccess>
{
    FilterListByAccess(bool isRemove): isRemove(isRemove) {}

    void operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        nx::vms::api::ResourceParamWithRefDataList& outList)
    {
        outList.erase(std::remove_if(
            outList.begin(),
            outList.end(),
            [&accessData, commonModule, this](const auto& param) -> bool
            {
                return !ModifyResourceParamAccess(isRemove)(commonModule, accessData, param);
            }),
            outList.end());
    }

    bool isRemove;
};

template<typename SingleAccess>
struct ModifyListAccess
{
    template<typename ParamContainer>
    Result operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const ParamContainer& paramContainer)
    {
        ParamContainer tmpContainer = paramContainer;
        FilterListByAccess<SingleAccess>()(commonModule, accessData, tmpContainer);
        if (paramContainer.size() != tmpContainer.size())
        {
            QString errorMessage = NX_FMT(
                "Modiying of %1 entries out of %2 is not allowed for %3",
                paramContainer.size() - tmpContainer.size(),
                paramContainer.size(),
                typeid(ParamContainer));
            return Result(ErrorCode::forbidden, std::move(errorMessage));
        }
        return Result();
    }
};

template<typename SingleAccess>
struct ReadListAccess
{
    template<typename ParamContainer>
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ParamContainer& paramContainer)
    {
        ParamContainer tmpContainer = paramContainer;
        FilterListByAccess<SingleAccess>()(commonModule, accessData, tmpContainer);
        return tmpContainer.size() != paramContainer.size() && tmpContainer.empty();
    }
};

template<typename SingleAccess>
struct ReadListAccessOut
{
    template<typename ParamContainer>
    RemotePeerAccess operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ParamContainer& paramContainer)
    {
        ParamContainer tmpContainer = paramContainer;
        FilterListByAccess<SingleAccess>()(commonModule, accessData, tmpContainer);
        if (paramContainer.size() != tmpContainer.size() && tmpContainer.empty())
            return RemotePeerAccess::Forbidden;
        if (tmpContainer.size() == paramContainer.size())
            return RemotePeerAccess::Allowed;
        return RemotePeerAccess::Partial;
    }
};

struct RegularTransactionType
{
    template<typename Param>
    ec2::TransactionType operator()(QnCommonModule*, const Param&, AbstractPersistentStorage*)
    {
        return TransactionType::Regular;
    }
};

struct LocalTransactionType
{
    template<typename Param>
    ec2::TransactionType operator()(QnCommonModule*, const Param&, AbstractPersistentStorage*)
    {
        return TransactionType::Local;
    }
};

template <typename ParamsType>
struct SetStatusTransactionType
{
    ec2::TransactionType operator()(
        QnCommonModule* commonModule,
        const ParamsType& params,
        AbstractPersistentStorage* db)
    {
        const auto isServer =
            [resourcePool = commonModule->resourcePool(), db, &id = params.id]() -> bool
            {
                if (QnResourcePtr resource = resourcePool->getResourceById<QnResource>(id))
                    return (bool) resource.dynamicCast<QnMediaServerResource>();
                return db->isServer(id);
            };
        return isServer() ? TransactionType::Local : TransactionType::Regular;
    }
};

#define TRANSACTION_DESCRIPTOR_APPLY( \
    _, \
    Key, \
    ParamType, \
    isPersistent, \
    isSystem, \
    isRemoveOperation, \
    getHashFunc, \
    triggerNotificationFunc, \
    checkSavePermissionFunc, \
    checkReadPermissionFunc, \
    filterBySavePermissionFunc, \
    filterByReadPermissionFunc, \
    checkRemotePeerAccessFunc, \
    getTransactionTypeFunc \
    ) \
    std::make_shared<TransactionDescriptor<ParamType>>( \
        ApiCommand::Key, \
        isPersistent, \
        isSystem, \
        isRemoveOperation, \
        #Key, \
        getHashFunc, \
        triggerNotificationFunc, \
        checkSavePermissionFunc, \
        checkReadPermissionFunc, \
        filterBySavePermissionFunc, \
        filterByReadPermissionFunc, \
        checkRemotePeerAccessFunc, \
        getTransactionTypeFunc),

DescriptorBaseContainer transactionDescriptors = {
    TRANSACTION_DESCRIPTOR_LIST(TRANSACTION_DESCRIPTOR_APPLY)
};

#undef TRANSACTION_DESCRIPTOR_APPLY

} // namespace detail

detail::TransactionDescriptorBase *getTransactionDescriptorByValue(ApiCommand::Value v)
{
    auto it = detail::transactionDescriptors.get<0>().find(v);
    bool isEnd = it == detail::transactionDescriptors.get<0>().end();
    NX_ASSERT(!isEnd, "ApiCommand::Value not found");
    return isEnd ? nullptr : (*it).get();
}

detail::TransactionDescriptorBase *getTransactionDescriptorByName(const QString& s)
{
    auto it = detail::transactionDescriptors.get<1>().find(s);
    bool isEnd = it == detail::transactionDescriptors.get<1>().end();
    return isEnd ? nullptr : (*it).get();
}

} // namespace ec2
