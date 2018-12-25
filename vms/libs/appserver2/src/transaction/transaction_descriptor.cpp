#include "transaction_descriptor.h"
#include "transaction_message_bus.h"

#include <iostream>

#include <api/global_settings.h>

#include <common/common_module.h>

#include <core/resource_access/user_access_data.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/param.h>
#include <core/resource_management/resource_pool.h>
#include <utils/license_usage_helper.h>

#include <nx/cloud/db/client/data/auth_data.h>

#include "ec_connection_notification_manager.h"

using namespace nx::vms;

namespace ec2 {

namespace detail {

struct InvalidGetHashHelper
{
    template<typename Param>
    QnUuid operator ()(const Param &)
    {
        // NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!");
        return QnUuid();
    }
};

struct InvalidTriggerNotificationHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &, const NotificationParams &)
    {
        NX_ASSERT(0, Q_FUNC_INFO, "This is a system transaction!"); // we MUSTN'T be here
    }
};

struct CreateHashByIdHelper
{
    template<typename Param>
    QnUuid operator ()(const Param &param) { return param.id; }
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
            //#ak no notification needed
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

struct UpdateNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.updatesNotificationManager->triggerNotification(tran, notificationParams.source);
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
    bool operator()(QnCommonModule*, const Qn::UserAccessData&, const Param&)
    {
        NX_ASSERT(0, lit("Invalid access check for this transaction (%1). We shouldn't be here.").arg(typeid(Param).name()));
        return false;
    }
};

struct InvalidAccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(QnCommonModule* commonModule, const Qn::UserAccessData&, const Param&)
    {
        NX_ASSERT(0, lit("Invalid outgoing transaction access check (%1). We shouldn't be here.").arg(typeid(Param).name()));
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
    bool operator()(QnCommonModule*, const Qn::UserAccessData&, const Param&) { return true; }
};

struct AllowForAllAccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(QnCommonModule*, const Qn::UserAccessData&, const Param&) { return RemotePeerAccess::Allowed; }
};

bool resourceAccessHelper(
    QnCommonModule* commonModule,
    const Qn::UserAccessData& accessData,
    const QnUuid& resourceId,
    Qn::Permissions permissions)
{
    if (hasSystemAccess(accessData))
        return true;
    const auto& resPool = commonModule->resourcePool();
    QnResourcePtr target = resPool->getResourceById(resourceId);
    auto userResource = resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
    if (commonModule->resourceAccessManager()->hasGlobalPermission(userResource, GlobalPermission::admin))
        return true;

    if (permissions == Qn::ReadPermission
        && accessData.access == Qn::UserAccessData::Access::ReadAllResources)
            return true;

    bool result = commonModule->resourceAccessManager()->hasPermission(userResource, target, permissions);
    if (!result)
        NX_DEBUG(typeid(TransactionDescriptorBase), lit("%1 \n\tuser %2 with %3 permissions is asking for \
                \n\t%4 resource with %5 permissions and fails...")
                .arg(Q_FUNC_INFO)
                .arg(accessData.userId.toString())
                .arg((int)accessData.access)
                .arg(resourceId.toString())
                .arg(permissions));

    return result;
}

struct ModifyResourceAccess
{
    ModifyResourceAccess(bool isRemove): isRemove(isRemove) {}

    template<typename Param>
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData,
        const Param& param)
    {
        NX_VERBOSE(this,
            lm("Got modify resource request. Is system access: %1, Data type: %2," \
                " Data contents: %3").args(hasSystemAccess(accessData), typeid(param).name(),
                    QJson::serialized(param)));

        if (hasSystemAccess(accessData))
            return true;

        const auto& resPool = commonModule->resourcePool();
        auto userResource = resPool->getResourceById(accessData.userId)
            .dynamicCast<QnUserResource>();
        QnResourcePtr target = resPool->getResourceById(param.id);

        bool result = false;
        if (isRemove)
            result = commonModule->resourceAccessManager()->hasPermission(userResource, target, Qn::RemovePermission);
        else if (!target)
            result = commonModule->resourceAccessManager()->canCreateResource(userResource, param);
        else
            result = commonModule->resourceAccessManager()->canModifyResource(userResource, target, param);

        if (!result)
            NX_WARNING(this, lit("%1 resource access returned false. User resource: %3. Target resource: %4")
                .arg(isRemove ? "Remove" : "Modify")
                .arg(userResource ? userResource->getId().toString() : QString())
                .arg(target ? target->getId().toString() : QString()));

        return result;
    }

    bool isRemove;
};

struct SaveUserAccess
{
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData,
        const nx::vms::api::UserData& param)
    {
        if (!hasSystemAccess(accessData))
        {
            auto allUsers = commonModule->resourcePool()->getResources<QnUserResource>();
            auto hasUserWithSameName = std::any_of(allUsers.cbegin(), allUsers.cend(),
                [&param](const auto& resPtr)
                { return resPtr->getName() == param.name && resPtr->getId() != param.id; });

            if (hasUserWithSameName)
            {
                NX_DEBUG(this, lm("Won't save user '%1' because of the name duplication")
                    .args(param.name));
                return false;
            }
        }

        return ModifyResourceAccess(/*isRemove*/ false)(commonModule, accessData, param);
    }
};

struct ModifyCameraDataAccess
{
    bool operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::CameraData& param)
    {
        if (!hasSystemAccess(accessData))
        {
            if (!param.physicalId.isEmpty() && !param.id.isNull())
            {
                auto expectedId = nx::vms::api::CameraData::physicalIdToId(param.physicalId);
                if (expectedId != param.id)
                    return false;
            }
        }

        return ModifyResourceAccess(/*isRemove*/ false)(commonModule, accessData, param);
    }
};

template<typename Param>
void applyColumnFilter(QnCommonModule*, const Qn::UserAccessData& /*accessData*/, Param& /*data*/) {}

void applyColumnFilter(
    QnCommonModule*, const Qn::UserAccessData& accessData, api::MediaServerData& data)
{
    if (accessData != Qn::kSystemAccess)
        data.authKey.clear();
}

void applyColumnFilter(
    QnCommonModule* commonModule, const Qn::UserAccessData& accessData, api::StorageData& data)
{
    if (!hasSystemAccess(accessData) && !commonModule->resourceAccessManager()->hasGlobalPermission(
        accessData, GlobalPermission::admin))
    {
        data.url = QnStorageResource::urlWithoutCredentials(data.url);
    }
}

struct ReadResourceAccess
{
    template<typename Param>
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, Param& param)
    {
        if (resourceAccessHelper(commonModule, accessData, param.id, Qn::ReadPermission))
        {
            applyColumnFilter(commonModule, accessData, param);
            return true;
        }
        return false;
    }
};

struct ReadResourceAccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const Param& param)
    {
        return resourceAccessHelper(commonModule, accessData, param.id, Qn::ReadPermission)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ReadResourceParamAccess
{
    bool operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        nx::vms::api::ResourceParamWithRefData& param)
    {
        if (resourceAccessHelper(commonModule, accessData, param.resourceId, Qn::ReadPermission))
        {
            operator()(
                commonModule,
                accessData,
                static_cast<nx::vms::api::ResourceParamData&>(param));
            return true;
        }
        return false;
    }

    bool operator()(
        QnCommonModule*,
        const Qn::UserAccessData& accessData,
        nx::vms::api::ResourceParamData& param)
    {
        namespace ahlp = access_helpers;
        ahlp::FilterFunctorListType filters = {
            static_cast<bool (*)(
                ahlp::Mode,
                const Qn::UserAccessData&,
                ahlp::KeyValueFilterType*)>(&ahlp::kvSystemOnlyFilter)
        };

        bool allowed;
        ahlp::KeyValueFilterType keyValue(param.name, &param.value);
        ahlp::applyValueFilters(ahlp::Mode::read, accessData, &keyValue, filters, &allowed);

        if (!allowed)
            return false;

        return true;
    }
};

struct ReadResourceParamAccessOut
{
    RemotePeerAccess operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::ResourceParamWithRefData& param)
    {
        return resourceAccessHelper(commonModule, accessData, param.resourceId, Qn::ReadPermission)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ModifyResourceParamAccess
{
    ModifyResourceParamAccess(bool isRemove): isRemove(isRemove) {}

    bool operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::ResourceParamWithRefData& param)
    {
        if (hasSystemAccess(accessData))
            return true;

        if (isRemove)
        {
            const auto& resPool = commonModule->resourcePool();
            commonModule->resourceAccessManager()->hasPermission(
                resPool->getResourceById<QnUserResource>(accessData.userId),
                resPool->getResourceById(param.resourceId),
                Qn::RemovePermission);
        }

        Qn::Permissions permissions = Qn::SavePermission;
        if (param.name == Qn::USER_FULL_NAME)
            permissions |= Qn::WriteFullNamePermission;

        return resourceAccessHelper(commonModule, accessData, param.resourceId, permissions);
    }

    bool isRemove;
};

struct ReadFootageDataAccess
{
    bool operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::ServerFootageData& param)
    {
        return resourceAccessHelper(
            commonModule,
            accessData,
            param.serverGuid,
            Qn::ReadPermission);
    }
};

struct ReadFootageDataAccessOut
{
    RemotePeerAccess operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::ServerFootageData& param)
    {
        return resourceAccessHelper(commonModule, accessData, param.serverGuid, Qn::ReadPermission)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ModifyFootageDataAccess
{
    bool operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::ServerFootageData& param)
    {
        return resourceAccessHelper(
            commonModule,
            accessData,
            param.serverGuid,
            Qn::SavePermission);
    }
};

struct ReadCameraAttributesAccess
{
    bool operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::CameraAttributesData& param)
    {
        return resourceAccessHelper(commonModule, accessData, param.cameraId, Qn::ReadPermission);
    }
};

struct ReadCameraAttributesAccessOut
{
    RemotePeerAccess operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::CameraAttributesData& param)
    {
        return resourceAccessHelper(commonModule, accessData, param.cameraId, Qn::ReadPermission)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ModifyCameraAttributesAccess
{
    bool operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::CameraAttributesData& param)
    {
        if (accessData == Qn::kSystemAccess)
            return true;

        if (!resourceAccessHelper(commonModule, accessData, param.cameraId, Qn::SavePermission))
        {
            qWarning() << "save CameraAttributesData forbidden because no save permissions. id="
                << param.cameraId;
            return false;
        }

        QnCamLicenseUsageHelper licenseUsageHelper(commonModule);
        QnVirtualCameraResourceList cameras;

        const auto& resPool = commonModule->resourcePool();
        auto camera = resPool->getResourceById(param.cameraId).dynamicCast<QnVirtualCameraResource
        >();
        if (!camera)
        {
            qWarning() <<
                "save CameraAttributesData forbidden because camera object is not exists. id="
                << param.cameraId;
            return false;
        }

        // Check the license if and only if recording goes from 'off' to 'on' state
        const bool prevScheduleEnabled = camera->isLicenseUsed();
        if (prevScheduleEnabled != param.scheduleEnabled)
        {
            licenseUsageHelper.propose(camera, param.scheduleEnabled);
            if (licenseUsageHelper.isOverflowForCamera(camera))
            {
                qWarning() <<
                    "save CameraAttributesData forbidden because no license to enable recording. id="
                    << param.cameraId;
                return false;
            }
        }
        return true;
    }
};

struct ModifyCameraAttributesListAccess
{
    void operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        nx::vms::api::CameraAttributesDataList& param)
    {
        if (accessData == Qn::kSystemAccess)
            return;

        for (const auto& p: param)
        {
            if (!resourceAccessHelper(commonModule, accessData, p.cameraId, Qn::SavePermission))
            {
                param = {};
                return;
            }
        }

        QnCamLicenseUsageHelper licenseUsageHelper(commonModule);
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
            const bool prevScheduleEnabled = camera->isLicenseUsed();
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
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData,
        const api::MediaServerUserAttributesData& param)
    {
        return resourceAccessHelper(commonModule, accessData, param.serverId, Qn::ReadPermission);
    }
};

struct ReadServerAttributesAccessOut
{
    RemotePeerAccess operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData,
        const api::MediaServerUserAttributesData& param)
    {
        return resourceAccessHelper(commonModule, accessData, param.serverId, Qn::ReadPermission)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ModifyServerAttributesAccess
{
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData,
        const api::MediaServerUserAttributesData& param)
    {
        return resourceAccessHelper(commonModule, accessData, param.serverId, Qn::SavePermission);
    }
};

struct UserInputAccess
{
    template<typename Param>
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const Param&)
    {
        if (hasSystemAccess(accessData))
            return true;

        const auto& resPool = commonModule->resourcePool();
        auto userResource = resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
        bool result = commonModule->resourceAccessManager()->hasGlobalPermission(userResource, GlobalPermission::userInput);
        return result;
    }
};

struct AdminOnlyAccess
{
    template<typename Param>
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const Param&)
    {
        if (hasSystemAccess(accessData))
            return true;

        const auto& resPool = commonModule->resourcePool();
        auto userResource = resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
        bool result = commonModule->resourceAccessManager()->hasGlobalPermission(userResource, GlobalPermission::admin);
        return result;
    }
};

struct AdminOnlyAccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const Param&)
    {
        if (hasSystemAccess(accessData))
            return RemotePeerAccess::Allowed;

        const auto& resPool = commonModule->resourcePool();
        auto userResource = resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
        RemotePeerAccess result = commonModule->resourceAccessManager()->hasGlobalPermission(userResource, GlobalPermission::admin)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
        return result;
    }
};

struct RemoveUserRoleAccess
{
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const nx::vms::api::IdData& param)
    {
        if (!AdminOnlyAccess()(commonModule, accessData, param))
        {
            qWarning() << "Removing user role is forbidden because the user has no admin access";
            return false;
        }

        const auto& resPool = commonModule->resourcePool();
        for (const auto& user: resPool->getResources<QnUserResource>())
        {
            if (user->userRoleId() == param.id)
            {
                qWarning() << "Removing user role is forbidden because the role is still used by the user " << user->getName();
                return false;
            }
        }

        return true;
    }
};

struct VideoWallControlAccess
{
    bool operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::VideowallControlMessageData&)
    {
        if (hasSystemAccess(accessData))
            return true;
        const auto& resPool = commonModule->resourcePool();
        auto userResource = resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource
        >();
        bool result = commonModule->resourceAccessManager()->hasGlobalPermission(
            userResource,
            GlobalPermission::controlVideowall);
        if (!result)
        {
            QString userName = userResource ? userResource->fullName() : lit("Unknown user");
            NX_WARNING(this, lit("Access check for ApiVideoWallControlMessageData failed for user %1")
                .arg(userName));
        }
        return result;
    }
};

struct LayoutTourAccess
{
    bool operator()(
        QnCommonModule* /*commonModule*/,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::LayoutTourData& tour)
    {
        return hasSystemAccess(accessData)
            || tour.parentId.isNull()
            || accessData.userId == tour.parentId;
    }
};

struct LayoutTourAccessById
{
    bool operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::IdData& tourId)
    {
        const auto tour = commonModule->layoutTourManager()->tour(tourId.id);
        return !tour.isValid() //< Allow everyone to work with tours which are already deleted.
            || LayoutTourAccess()(commonModule, accessData, tour);
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
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        const Param& param)
    {
        return SingleAccess()(commonModule, accessData, param)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

template<typename SingleAccess >
struct FilterListByAccess
{
    template<typename ParamContainer>
    void operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, ParamContainer& outList)
    {
        outList.erase(std::remove_if(outList.begin(), outList.end(),
            [&accessData, commonModule](typename ParamContainer::value_type &param)
        {
            return !SingleAccess()(commonModule, accessData, param);
        }), outList.end());
    }
};

template<>
struct FilterListByAccess<ModifyResourceAccess>
{
    FilterListByAccess(bool isRemove): isRemove(isRemove) {}

    template<typename ParamContainer>
    void operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, ParamContainer& outList)
    {
        outList.erase(std::remove_if(outList.begin(), outList.end(),
            [&accessData, this, commonModule](const typename ParamContainer::value_type &param)
        {
            return !ModifyResourceAccess(isRemove)(commonModule, accessData, param);
        }), outList.end());
    }

    bool isRemove;
};

template<>
struct FilterListByAccess<ModifyResourceParamAccess>
{
    FilterListByAccess(bool isRemove): isRemove(isRemove)
    {
    }

    void operator()(
        QnCommonModule* commonModule,
        const Qn::UserAccessData& accessData,
        nx::vms::api::ResourceParamWithRefDataList& outList)
    {
        outList.erase(
            std::remove_if(
                outList.begin(),
                outList.end(),
                [&accessData, commonModule, this](
                const nx::vms::api::ResourceParamWithRefData& param)
                    {
                        return !ModifyResourceParamAccess(isRemove)(
                            commonModule,
                            accessData,
                            param);
                    }),
            outList.end());
    }

    bool isRemove;
};

template<typename SingleAccess>
struct ModifyListAccess
{
    template<typename ParamContainer>
    bool operator()(QnCommonModule* commonModule, const Qn::UserAccessData& accessData, const ParamContainer& paramContainer)
    {
        ParamContainer tmpContainer = paramContainer;
        FilterListByAccess<SingleAccess>()(commonModule, accessData, tmpContainer);
        if (paramContainer.size() != tmpContainer.size())
        {
            NX_WARNING(this, lit("Modify list access filtered out %1 entries from %2. Transaction: %3")
                .arg(paramContainer.size() - tmpContainer.size())
                .arg(paramContainer.size())
                .arg(typeid(ParamContainer)));
            return false;
        }
        return true;
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
    ec2::TransactionType::Value operator()(QnCommonModule*, const Param&, AbstractPersistentStorage*)
    {
        return TransactionType::Regular;
    }
};

struct CloudTransactionType
{
    template<typename Param>
    ec2::TransactionType::Value operator()(QnCommonModule*, const Param&, AbstractPersistentStorage*)
    {
        return TransactionType::Cloud;
    }
};

struct LocalTransactionType
{
    template<typename Param>
    ec2::TransactionType::Value operator()(QnCommonModule*, const Param&, AbstractPersistentStorage*)
    {
        return TransactionType::Local;
    }
};

ec2::TransactionType::Value getStatusTransactionTypeFromDb(
    const QnUuid& id, AbstractPersistentStorage* db)
{
    api::MediaServerData server = db->getServer(id);
    if (server.id.isNull())
        return ec2::TransactionType::Unknown;
    return TransactionType::Local;
}

struct SetStatusTransactionType
{
    ec2::TransactionType::Value operator()(
        QnCommonModule* commonModule,
        const nx::vms::api::ResourceStatusData& params,
        AbstractPersistentStorage* db)
    {
        const auto& resPool = commonModule->resourcePool();
        QnResourcePtr resource = resPool->getResourceById<QnResource>(params.id);
        if (!resource)
            return getStatusTransactionTypeFromDb(params.id, db);
        if (resource.dynamicCast<QnMediaServerResource>())
            return TransactionType::Local;
        else
            return TransactionType::Regular;
    }
};

struct SaveUserTransactionType
{
    ec2::TransactionType::Value operator()(QnCommonModule*, const nx::vms::api::UserData& params,
        AbstractPersistentStorage* /*db*/)
    {
        return params.isCloud ? TransactionType::Cloud : TransactionType::Regular;
    }
};

struct SetResourceParamTransactionType
{
    ec2::TransactionType::Value operator()(
        QnCommonModule*,
        const nx::vms::api::ResourceParamWithRefData& param,
        AbstractPersistentStorage* /*db*/)
    {
        if (param.resourceId == QnUserResource::kAdminGuid &&
            param.name == nx::settings_names::kNameSystemName)
        {
            // System rename MUST be propagated to Nx Cloud
            return TransactionType::Cloud;
        }

        if (param.name == nx::cloud::db::api::kVmsUserAuthInfoAttributeName ||
            param.name == Qn::USER_FULL_NAME)
        {
            return TransactionType::Cloud;
        }

        return TransactionType::Regular;
    }
};

ec2::TransactionType::Value getRemoveUserTransactionTypeFromDb(
    const QnUuid& id,
    AbstractPersistentStorage* db)
{
    nx::vms::api::UserData userData = db->getUser(id);
    if (userData.id.isNull())
        return ec2::TransactionType::Unknown;

    return userData.isCloud ? TransactionType::Cloud : TransactionType::Regular;
}

struct RemoveUserTransactionType
{
    ec2::TransactionType::Value operator()(QnCommonModule* commonModule, const nx::vms::api::IdData& params, AbstractPersistentStorage* db)
    {
        const auto& resPool = commonModule->resourcePool();
        auto user = resPool->getResourceById<QnUserResource>(params.id);
        if (!user)
            return getRemoveUserTransactionTypeFromDb(params.id, db);
        return user->isCloud() ? TransactionType::Cloud : TransactionType::Regular;
    }
};

#define TRANSACTION_DESCRIPTOR_APPLY( \
    _, \
    Key, \
    ParamType, \
    isPersistent, \
    isSystem, \
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
        #Key, \
        getHashFunc, \
        [](const QnAbstractTransaction &tran) { return QnTransaction<ParamType>(tran); },  \
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
