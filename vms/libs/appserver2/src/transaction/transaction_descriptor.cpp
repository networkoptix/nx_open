// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "transaction_descriptor.h"

#include <iostream>

#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_property_key.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_access/user_access_data.h>
#include <core/resource_management/resource_pool.h>
#include <nx/branding.h>
#include <nx/cloud/db/client/data/auth_data.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/api/data/backup_settings.h>
#include <nx/vms/api/data/storage_flags.h>
#include <nx/vms/api/data/storage_space_data.h>
#include <nx/vms/api/data/user_model.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_service_usage_helper.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/ec2/ec_connection_notification_manager.h>
#include <nx/vms/license/usage_helper.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/data/api_fwd.h>

#include "amend_transaction_data.h"

using namespace nx::vms;
using SystemContext = nx::vms::common::SystemContext;

namespace ec2 {

namespace detail {

namespace {

struct ServerApiErrors
{
    Q_DECLARE_TR_FUNCTIONS(ServerApiErrors);
};

class CanAddOrRemoveParentGroups final
{
public:
    /**
     * Checks whether a new or existing User can be added to the given parent User Groups.
     * @param manager
     * @param accessData
     * @param existingUser existing User. Value `nullptr` is considered as a new user to be
     * created.
     * @param parentGroups parent User Groups to add the User into
     * @return
     */
    [[nodiscard]] Result operator()(nx::vms::common::UserGroupManager* const manager,
        const Qn::UserAccessData& accessData,
        const QnSharedResourcePointer<QnUserResource>& existingUser,
        const std::vector<QnUuid>& parentGroups) const
    {
        return canAddOrRemove(manager, accessData, existingUser, parentGroups);
    }

    /**
     * Checks whether a new or existing User Group can be added to the given parent User Groups.
     * @param manager
     * @param accessData
     * @param existingGroup existing User Group. Value `nullptr` is considered as a new User Group
     * to be created.
     * @param parentGroups parent User Groups to add the User Group into
     * @return
     */
    [[nodiscard]] Result operator()(nx::vms::common::UserGroupManager* const manager,
        const Qn::UserAccessData& accessData,
        const std::optional<api::UserGroupData>& existingGroup,
        const std::vector<QnUuid>& parentGroups) const
    {
        return canAddOrRemove(manager, accessData, existingGroup, parentGroups);
    }

private:
    template<typename ChildT>
    [[nodiscard]] Result canAddOrRemove(nx::vms::common::UserGroupManager* const manager,
        const Qn::UserAccessData& /*accessData*/,
        const ChildT& existingChild,
        std::vector<QnUuid> parentIds) const
    {
        if (!NX_ASSERT(manager))
            return {ErrorCode::serverError};

        // returns elements from `rangeToSearch` which are not found in `rangeContaining`
        // TODO: #skolesnik Consider moving to `open/libs/nx_utils/src/nx/utils/std/algorithm.h`
        const auto setDifference =
            [](const auto& rangeToSearch, const auto& rangeContaining)
            {
                std::vector<QnUuid> notFound;
                std::set_difference(rangeToSearch.cbegin(), rangeToSearch.cend(),
                    rangeContaining.cbegin(), rangeContaining.cend(),
                    std::back_inserter(notFound));
                return notFound;
            };

        const auto selectGroupProps =
            [](const api::UserGroupData& groupData)
            {
                struct
                {
                    QnUuid id;
                    QString name;
                    api::UserType type;
                } selected{.id = groupData.id, .name = groupData.name, .type = groupData.type};

                return selected;
            };

        using nx::utils::unique_sorted;

        const auto existing = unique_sorted(parentGroups(existingChild));
        const auto updated = unique_sorted(std::move(parentIds));

        const auto newParents = manager->mapGroupsByIds(
            setDifference(updated, existing),
            selectGroupProps);

        const auto removeParents = manager->mapGroupsByIds(
            setDifference(existing, updated),
            selectGroupProps);

        for (const auto& parent: newParents)
        {
            if (!parent.value)
            {
                return {ErrorCode::badRequest,
                    nx::format(ServerApiErrors::tr("User Group '%1' does not exist."),
                        parent.id.toString())};
            }
        }

        for (const auto& parentGroup: removeParents)
        {
            if (!parentGroup.value)
            {
                NX_WARNING(this,
                    "Requested to remove from non-existing User Group '%1'.",
                    parentGroup.id);
                continue;
            }
        }

        for (const auto& parent: newParents)
        {
            if (parent.value->type == api::UserType::ldap)
                return errorCantAddToLdap(existingChild, *parent.value);
        }

        for (const auto& parent: removeParents)
        {
            if (parent.value->type == api::UserType::ldap)
                return errorCantRemoveFromLdap(existingChild, *parent.value);
        }

        return {};
    }

    static std::vector<QnUuid> parentGroups(
        const QnSharedResourcePointer<QnUserResource>& existingUser)
    {
        return existingUser ? existingUser->groupIds() : std::vector<QnUuid>{};
    }

    static std::vector<QnUuid> parentGroups(
        const std::optional<api::UserGroupData>& existingGroup)
    {
        return existingGroup ? existingGroup->parentGroupIds : std::vector<QnUuid>{};
    }

    template<typename GroupInfo>
    static Result errorCantAddToLdap(
        const QnSharedResourcePointer<QnUserResource>& /*existingUser*/, const GroupInfo& group)
    {
        return {ErrorCode::forbidden,
            nx::format(
                ServerApiErrors::tr(
                    "Forbidden to add any Users to a LDAP User Group '%1(%2)'"),
                group.name,
                group.id.toString())};
    }

    template<typename GroupInfo>
    static Result errorCantRemoveFromLdap(
        const QnSharedResourcePointer<QnUserResource>& /*existingUser*/, const GroupInfo& group)
    {
        return {ErrorCode::forbidden,
            nx::format(
                ServerApiErrors::tr(
                    "Forbidden to remove any Users from a LDAP User Group '%1(%2)'"),
                group.name,
                group.id.toString())};
    }

    template<typename GroupInfo>
    static Result errorCantAddToLdap(
        const std::optional<api::UserGroupData>& /*existingGroup*/, const GroupInfo& group)
    {
        return {ErrorCode::forbidden,
            nx::format(
                ServerApiErrors::tr(
                    "Forbidden to add any User Groups to a LDAP User Group '%1(%2)'"),
                group.name,
                group.id.toString())};
    }

    template<typename GroupInfo>
    static Result errorCantRemoveFromLdap(
        const std::optional<api::UserGroupData>& /*existingGroup*/, const GroupInfo& group)
    {
        return {ErrorCode::forbidden,
            nx::format(
                ServerApiErrors::tr(
                    "Forbidden to remove any User Groups from a LDAP User Group '%1(%2)'"),
                group.name,
                group.id.toString())};
    }

} const canAddOrRemoveParentGroups{};

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
        case ApiCommand::removeUserGroup:
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
        case ApiCommand::removeShowreel:
            return notificationParams.showreelNotificationManager->triggerNotification(
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
        case ApiCommand::removeAnalyticsPlugin:
        case ApiCommand::removeAnalyticsEngine:
            return notificationParams.analyticsNotificationManager->triggerNotification(
                tran,
                notificationParams.source);
            break;
        case ApiCommand::removeLookupList:
            return notificationParams.lookupListNotificationManager->triggerNotification(
                tran,
                notificationParams.source);
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

struct ShowreelNotificationManagerHelper
{
    template<typename Param>
    void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
    {
        notificationParams.showreelNotificationManager->triggerNotification(tran, notificationParams.source);
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

struct LookupListNotificationManagerHelper
{
    template<typename Param>
    void operator()(const QnTransaction<Param>& tran, const NotificationParams& notificationParams)
    {
        notificationParams.lookupListNotificationManager->triggerNotification(
            tran, notificationParams.source);
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
    Result operator()(SystemContext*, const Qn::UserAccessData&, const Param&)
    {
        NX_ASSERT(false, "Invalid access check for %1", typeid(Param));
        return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
            "Invalid access check for %1.", /*comment*/ "%1 is a C++ type name"), typeid(Param)));
    }
};

struct InvalidAccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(SystemContext* systemContext,
        const Qn::UserAccessData&, const Param&)
    {
        NX_ASSERT(false,
            "Invalid outgoing transaction access check (%1).", typeid(Param));
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
    bool operator()(SystemContext*, const Qn::UserAccessData& accessData, const Param&)
    {
        return hasSystemAccess(accessData);
    }
};

struct SystemSuperUserAccessOnlyOut
{
    template<typename Param>
    RemotePeerAccess operator()(SystemContext*, const Qn::UserAccessData& accessData, const Param&)
    {
        return hasSystemAccess(accessData)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct AllowForAllAccess
{
    template<typename Param>
    Result operator()(SystemContext*, const Qn::UserAccessData&, const Param&)
    {
        return Result();
    }
};

struct AllowForAllAccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(SystemContext*, const Qn::UserAccessData&, const Param&) { return RemotePeerAccess::Allowed; }
};

static Result checkExistingResourceAccess(
    SystemContext* systemContext,
    const Qn::UserAccessData& accessData,
    const QnUuid& resourceId,
    Qn::Permissions permissions)
{
    const auto resPool = systemContext->resourcePool();
    auto userResource = resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();

    // Null resource Id can not be handled by permissions engine, since there is no such resource.
    // System settings are stored as admin user properties
    if ((resourceId.isNull() || resourceId == QnUserResource::kAdminGuid)
        && userResource
        && systemContext->resourceAccessManager()->hasPowerUserPermissions(userResource))
    {
        return Result();
    }

    QnResourcePtr target = resPool->getResourceById(resourceId);
    if (systemContext->resourceAccessManager()->hasPermission(
        userResource, target, permissions))
    {
        return Result();
    }

    return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
        "Request of User %1 with %2 permissions for %3 Resource with %4 permissions is forbidden."),
        userResource ? userResource->getName() : accessData.userId.toString(),
        accessData.access,
        resourceId,
        permissions));
}

static Result checkReadResourceAccess(
    SystemContext* systemContext, const Qn::UserAccessData& accessData, const QnUuid& resourceId)
{
    if (hasSystemAccess(accessData))
        return Result();

    if (accessData.access == Qn::UserAccessData::Access::ReadAllResources)
        return Result();

    return checkExistingResourceAccess(systemContext, accessData, resourceId, Qn::ReadPermission);
}

static Result checkSaveResourceAccess(
    SystemContext* systemContext, const Qn::UserAccessData& accessData, const QnUuid& resourceId)
{
    if (hasSystemAccess(accessData))
        return Result();

    return checkExistingResourceAccess(systemContext, accessData, resourceId, Qn::SavePermission);
}

static Result checkReadResourceParamAccess(
    SystemContext* systemContext, const Qn::UserAccessData& accessData, const QnUuid& resourceId)
{
    return checkReadResourceAccess(systemContext, accessData, resourceId);
}

struct ModifyResourceAccess
{
    ModifyResourceAccess() {}

    template<typename Param>
    Result operator()(SystemContext* systemContext, const Qn::UserAccessData& accessData,
        const Param& param)
    {
        NX_VERBOSE(this,
            "Got modify resource request. Is system access: %1, Data type: %2, Data contents: %3",
            hasSystemAccess(accessData), typeid(param), QJson::serialized(param));

        if (hasSystemAccess(accessData))
            return Result();

        const auto& resPool = systemContext->resourcePool();
        auto userResource = resPool->getResourceById(accessData.userId)
            .dynamicCast<QnUserResource>();
        QnResourcePtr target = resPool->getResourceById(param.id);

        bool result = false;
        if (!target)
            result = systemContext->resourceAccessManager()->canCreateResourceFromData(userResource, param);
        else
            result = systemContext->resourceAccessManager()->canModifyResource(userResource, target, param);

        if (!result)
        {
            QString errorMessage = target
                ? nx::format(ServerApiErrors::tr("User '%1' is not permitted to modify %2."),
                    userResource ? userResource->getName() : QString(),
                    target->getId().toSimpleString())
                : nx::format(ServerApiErrors::tr("User '%1' is not permitted to create new resource."),
                    userResource ? userResource->getName() : QString());
            return Result(ErrorCode::forbidden, std::move(errorMessage));
        }

        auto typeDescriptor = qnResTypePool->getResourceType(param.typeId);
        if (!typeDescriptor)
        {
            return Result(ErrorCode::badRequest, nx::format(ServerApiErrors::tr(
                "Invalid Resource type %1."),
                param.typeId));
        }

        return Result();
    }
};

struct ModifyStorageAccess
{
    Result operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::StorageData& param)
    {
        if (param.url.isEmpty())
        {
            NX_DEBUG(this, "Declining save storage request because provided url is empty");
            return Result(ErrorCode::badRequest, ServerApiErrors::tr(
                "Empty Storage URL is not allowed."));
        }

        transaction_descriptor::CanModifyStorageData data;
        const auto existingResource = systemContext->resourcePool()->getResourceById(param.id);
        data.hasExistingStorage = (bool) existingResource;
        data.storageType = param.storageType;
        data.isBackup = param.isBackup;
        data.getExistingStorageDataFunc =
            [&]()
            {
                nx::vms::api::ResourceData result;
                fromResourceToApi(existingResource, result);
                return result;
            };

        data.logErrorFunc = [this](const QString& message) { NX_DEBUG(this, message); };
        data.modifyResourceResult = ModifyResourceAccess()(systemContext, accessData, param);
        data.request = param;
        amendOutputDataIfNeeded(accessData, systemContext->resourceAccessManager(), &data.request);

        return transaction_descriptor::canModifyStorage(data);
    }
};

struct RemoveResourceAccess
{
    RemoveResourceAccess()  {}

    template<typename Param>
    Result operator()(SystemContext* systemContext, const Qn::UserAccessData& accessData,
        const Param& param)
    {
        NX_VERBOSE(this,
            "Got remove resource request. Is system access: %1, Data type: %2, Data contents: %3",
            hasSystemAccess(accessData), typeid(param), QJson::serialized(param));

        if (hasSystemAccess(accessData))
            return Result();

        const auto& resPool = systemContext->resourcePool();
        auto userResource = resPool->getResourceById(accessData.userId)
            .dynamicCast<QnUserResource>();
        QnResourcePtr target = resPool->getResourceById(param.id);

        if (const auto user = target.dynamicCast<QnUserResource>();
            user && user->attributes().testFlag(nx::vms::api::UserAttribute::readonly))
        {
            return Result(ErrorCode::forbidden, ServerApiErrors::tr(
                "Removal of the User with readonly attribute is forbidden for VMS."));
        }

        if (!systemContext->resourceAccessManager()->hasPermission(
            userResource, target, Qn::RemovePermission))
        {
            return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
                "User '%1' is not permitted to remove %2."),
                userResource ? userResource->getName() : QString(),
                target ? target->getId().toSimpleString() : QString()));
        }

        const auto storage = target.dynamicCast<QnStorageResource>();
        bool isOwnStorage = (storage && storage->getParentId() == systemContext->peerId());
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
    Result operator()(SystemContext* systemContext, const Qn::UserAccessData& accessData,
        const nx::vms::api::UserData& param)
    {
        if (hasSystemAccess(accessData))
            return ModifyResourceAccess()(systemContext, accessData, param);

        auto resourcePool = systemContext->resourcePool();

        // Allow to edit disabled user even if it is clashed by name.
        if (param.isEnabled)
        {
            const bool hasUserWithSameName = resourcePool->contains<QnUserResource>(
                [name = param.name.toLower(), id = param.id](const auto& u)
                {
                    return u->isEnabled() && u->getName().toLower() == name && u->getId() != id;
                });
            if (hasUserWithSameName)
            {
                return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
                    "User '%1' will not be saved because of the login duplication."), param.name));
            }
        }

        const auto existingUser = resourcePool->getResourceById<QnUserResource>(param.id);
        if (existingUser)
        {
            if (existingUser->attributes().testFlag(nx::vms::api::UserAttribute::readonly))
            {
                return Result(ErrorCode::forbidden, ServerApiErrors::tr(
                    "Change of the User with readonly attribute is forbidden for VMS."));
            }

            if (existingUser->externalId() != param.externalId)
            {
                return Result(ErrorCode::forbidden, ServerApiErrors::tr(
                    "Change of `externalId` is forbidden."));
            }

            if (param.digest != nx::vms::api::UserData::kHttpIsDisabledStub
                && (param.type == nx::vms::api::UserType::cloud
                    || existingUser->getDigest() == param.digest)
                && existingUser->getName() != param.name)
            {
                return Result(ErrorCode::forbidden,
                    (param.type == nx::vms::api::UserType::cloud)
                        ? nx::format(ServerApiErrors::tr(
                            "User '%1' will not be saved because names differ: "
                            "'%1' vs '%2' and changing name is forbidden for %3 users.",
                            /*comment*/ "%3 is the short Cloud name"),
                            existingUser->getName(), param.name, nx::branding::shortCloudName())
                        : nx::format(ServerApiErrors::tr(
                            "User '%1' will not be saved because names differ: "
                            "'%1' vs '%2' and `password` has not been provided."),
                            existingUser->getName(), param.name));
            }
        }
        else
        {
            if (param.type == nx::vms::api::UserType::ldap)
            {
                return Result(
                    ErrorCode::forbidden, ServerApiErrors::tr("LDAP User creation is forbidden."));
            }

            if (param.name.isEmpty())
            {
                return Result(ErrorCode::badRequest, ServerApiErrors::tr(
                    "User with empty name is not allowed."));
            }
        }

        if (!existingUser && param.isAdministrator())
        {
            return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
                "Creating an Administrator user is not allowed.")));
        }

        if (const auto res = canAddOrRemoveParentGroups(
                    systemContext->userGroupManager(),
                    accessData,
                    existingUser,
                    param.groupIds);
            !res)
        {
            return res;
        }

        if (param.type == nx::vms::api::UserType::cloud
            && ((existingUser && param.fullName != existingUser->fullName())
                || (!existingUser && !param.fullName.isEmpty())))
        {
            return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
                "User full name is controlled by the %1.", /*comment*/ "%1 is the short Cloud name"),
                nx::branding::shortCloudName()));
        }

        if (param.type == nx::vms::api::UserType::temporaryLocal && existingUser)
        {
            const auto temporaryToken = QJson::deserialized<nx::vms::api::TemporaryToken>(param.hash);
            if (!temporaryToken.isValid())
            {
                return Result(
                    ErrorCode::forbidden,
                    ServerApiErrors::tr("Invalid temporary token"));
            }
        }

        return ModifyResourceAccess()(systemContext, accessData, param);
    }

    Result operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::UserDataDeprecated& param)
    {
        return (*this)(systemContext, accessData, param.toUserData());
    }
};

struct ModifyCameraDataAccess
{
    Result operator()(
        SystemContext* systemContext,
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
                    return Result(ErrorCode::badRequest, nx::format(ServerApiErrors::tr(
                        "Expected Device id %1 is not equal to actual id %2 (`physicalId` %3)."),
                        expectedId, param.id, param.physicalId));
                }
            }
        }

        return ModifyResourceAccess()(systemContext, accessData, param);
    }
};

template<typename Param>
void applyColumnFilter(SystemContext*, const Qn::UserAccessData& /*accessData*/, Param& /*data*/)
{
}

void applyColumnFilter(
    SystemContext*, const Qn::UserAccessData& accessData, api::MediaServerData& data)
{
    if (accessData != Qn::kSystemAccess)
        data.authKey.clear();
}

void applyColumnFilter(SystemContext* /*systemContext*/,
    const Qn::UserAccessData& accessData,
    api::MediaServerDataEx& data)
{
    if (accessData != Qn::kSystemAccess)
        data.authKey.clear();
}

void applyColumnFilter(
    SystemContext* systemContext, const Qn::UserAccessData& accessData, api::StorageData& data)
{
    if (!hasSystemAccess(accessData)
        && !systemContext->resourceAccessManager()->hasPowerUserPermissions(accessData))
    {
        data.url = QnStorageResource::urlWithoutCredentials(data.url);
    }
}

struct ReadResourceAccess
{
    template<typename Param>
    Result operator()(SystemContext* systemContext, const Qn::UserAccessData& accessData, Param& param)
    {
        if (const auto r = checkReadResourceAccess(systemContext, accessData, param.id); !r)
            return r;

        applyColumnFilter(systemContext, accessData, param);
        return Result();
    }
};

struct ReadResourceAccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(SystemContext* systemContext, const Qn::UserAccessData& accessData, const Param& param)
    {
        return checkReadResourceAccess(systemContext, accessData, param.id)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ReadResourceParamAccess
{
    Result operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        nx::vms::api::ResourceParamWithRefData& param)
    {
        if (const auto r =
            checkReadResourceParamAccess(systemContext, accessData, param.resourceId)
            ;
            !r)
        {
            return r;
        }
        operator()(
            systemContext,
            accessData,
            static_cast<nx::vms::api::ResourceParamData&>(param));
        return Result();
    }

    Result operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        nx::vms::api::ResourceParamData& param)
    {
        const auto accessManager = systemContext->resourceAccessManager();
        if (accessData == Qn::kSystemAccess
            || accessData.access == Qn::UserAccessData::Access::ReadAllResources
            || accessManager->hasPowerUserPermissions(accessData))
        {
            return Result();
        }

        return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
            "Request of User %1 with %2 permissions to read Resource parameter `%3` is forbidden."),
            accessData.userId, accessData.access, param.name));
    }
};

struct ReadResourceParamAccessOut
{
    RemotePeerAccess operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::ResourceParamWithRefData& param)
    {
        return checkReadResourceParamAccess(systemContext, accessData, param.resourceId)
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

    static QString userNameOrId(const Qn::UserAccessData& accessData, SystemContext* systemContext)
    {
        auto user = systemContext->resourcePool()->getResourceById<QnUserResource>(accessData.userId);
        return userNameOrId(user, accessData);
    }

    static bool hasSameProperty(
        const QnResourcePtr& target, const nx::vms::api::ResourceParamWithRefData& param)
    {
        auto value = target->getProperty(param.name);
        return !value.isNull() && value == param.value;
    }

    static bool hasSameProperty(
        SystemContext* systemContext, const nx::vms::api::ResourceParamWithRefData& param)
    {
        auto target = systemContext->resourcePool()->getResourceById(param.resourceId);
        return target && hasSameProperty(target, param);
    }

    ModifyResourceParamAccess(bool isRemove): isRemove(isRemove) {}

    Result checkAnalyticsIntegrationAccess(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::ResourceParamWithRefData& param)
    {
        const auto resPool = systemContext->resourcePool();
        const auto camera = resPool->getResourceById<QnVirtualCameraResource>(param.resourceId);
        if (!NX_ASSERT(camera))
        {
            return Result(ErrorCode::serverError, nx::format(ServerApiErrors::tr(
                "Camera %1 does not exist."), param.resourceId));
        }

        QSet<QnUuid> newEngines = camera->calculateUserEnabledAnalyticsEngines(param.value);
        nx::vms::common::saas::IntegrationServiceUsageHelper helper(systemContext);
        helper.proposeChange(param.resourceId, newEngines);
        if (helper.isOverflow())
        {
            return Result(ErrorCode::serverError, nx::format(ServerApiErrors::tr(
                "Not enough integration licenses for camera %1."), param.resourceId));
        }

        return Result();
    }

    Result checkMetadataStorageAccess(
        SystemContext* systemContext,
        const Qn::UserAccessData& /*accessData*/,
        const nx::vms::api::ResourceParamWithRefData& param)
    {
        const auto resPool = systemContext->resourcePool();
        const auto metadataStorageId = QnUuid::fromStringSafe(param.value);
        const auto server = resPool->getResourceById<QnMediaServerResource>(param.resourceId);
        if (!NX_ASSERT(server))
        {
            return Result(ErrorCode::serverError, nx::format(ServerApiErrors::tr(
                "Server %1 does not exist."), param.resourceId));
        }

        const auto storages = server->getStorages();
        const auto storage = nx::utils::find_if(storages,
            [&metadataStorageId](const auto& s) { return s->getId() == metadataStorageId; });

        if (!storage)
        {
            return Result(ErrorCode::badRequest, nx::format(ServerApiErrors::tr(
                "Storage %1 does not belong to this Server."), param.value));
        }

        if (!((*storage)->persistentStatusFlags().testFlag(nx::vms::api::StoragePersistentFlag::dbReady)))
        {
            NX_DEBUG(
                this, "%1: Storage %2 is not db ready. Returning forbidden.",
                __func__, nx::utils::url::hidePassword((*storage)->getUrl()));
            return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
                "Storage %1 can not store analytics."), param.value));
        }

        NX_DEBUG(
            this, "%1: Storage %2 is db ready. Returning OK.",
            __func__, nx::utils::url::hidePassword((*storage)->getUrl()));
        return Result();
    }

    Result operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::ResourceParamWithRefData& param)
    {
        if (hasSystemAccess(accessData))
            return Result();

        const bool isNewApiCompoundTransaction =
            param.checkResourceExists != nx::vms::api::CheckResourceExists::yes;

        if (!isRemove
            && isNewApiCompoundTransaction
            && hasSameProperty(systemContext, param))
        {
            // CRUD API PATCH merges with existing values represented as JSON object so some of
            // them are not changed.
            return Result();
        }

        if (kSystemAccessOnlyProperties.find(param.name) != kSystemAccessOnlyProperties.cend())
        {
            return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
                "User '%1' with %2 permissions is not allowed to modify Resource parameter '%3'."),
                userNameOrId(accessData, systemContext), accessData.access, param.name));
        }

        const auto accessManager = systemContext->resourceAccessManager();
        const auto hasPowerUserPermissions = accessManager->hasPowerUserPermissions(accessData);

        // System properties are stored in resource unrelated places and should be handled
        // differently.
        if (hasPowerUserPermissions)
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

        const auto resPool = systemContext->resourcePool();
        const auto target = resPool->getResourceById(param.resourceId);

        if (!isRemove)
        {
            Result result;
            if (param.name == ResourcePropertyKey::Server::kMetadataStorageIdKey)
                result = checkMetadataStorageAccess(systemContext, accessData, param);
            if (param.name == QnVirtualCameraResource::kUserEnabledAnalyticsEnginesProperty)
                result = checkAnalyticsIntegrationAccess(systemContext, accessData, param);
            if (result.error != ErrorCode::ok)
                return result;
        }

        if (isNewApiCompoundTransaction && hasPowerUserPermissions)
            return Result();

        Qn::Permissions permissions = Qn::SavePermission;
        if (param.name == Qn::USER_FULL_NAME)
            permissions |= Qn::WriteFullNamePermission;

        return accessManager->hasPermission(accessData, target, permissions)
            ? Result()
            : Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
                "User '%1' with %2 permissions is not allowed to modify Resource parameter of %3."),
                userNameOrId(accessData, systemContext), accessData.access, param.resourceId));
    }

    bool isRemove;
};

const std::set<QString> ModifyResourceParamAccess::kSystemAccessOnlyProperties = {
    ResourcePropertyKey::kForcedLicenseType
};

struct ReadFootageDataAccess
{
    Result operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::ServerFootageData& param)
    {
        return checkReadResourceAccess(systemContext, accessData, param.serverGuid);
    }
};

struct ReadFootageDataAccessOut
{
    RemotePeerAccess operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::ServerFootageData& param)
    {
        return checkReadResourceAccess(systemContext, accessData, param.serverGuid)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ModifyFootageDataAccess
{
    Result operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::ServerFootageData& param)
    {
        return checkSaveResourceAccess(systemContext, accessData, param.serverGuid);
    }
};

struct ReadCameraAttributesAccess
{
    Result operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::CameraAttributesData& param)
    {
        return checkReadResourceAccess(systemContext, accessData, param.cameraId);
    }
};

struct ReadCameraAttributesAccessOut
{
    RemotePeerAccess operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::CameraAttributesData& param)
    {
        return checkReadResourceAccess(systemContext, accessData, param.cameraId)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ModifyCameraAttributesAccess
{
    Result operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::CameraAttributesData& param)
    {
        if (hasSystemAccess(accessData))
            return Result();

        const auto& resPool = systemContext->resourcePool();
        auto accessManager = systemContext->resourceAccessManager();
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
                return Result(ErrorCode::forbidden, ServerApiErrors::tr(
                    "`scheduleEnabled` set to true is not allowed for Device creation."));
            }

            if (param.checkResourceExists != nx::vms::api::CheckResourceExists::yes)
            {
                if (accessManager->hasPowerUserPermissions(accessData))
                    return Result();
            }
        }

        auto user = resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
        if (!accessManager->hasPermission(user, camera, Qn::SavePermission))
        {
            return Result(ErrorCode::forbidden, ServerApiErrors::tr(
                "Saving Device attributes is forbidden: no saving permission."));
        }

        // Check the license if and only if recording goes from 'off' to 'on' state
        if (param.scheduleEnabled && !camera->isScheduleEnabled())
        {
            using namespace nx::vms::license;
            CamLicenseUsageHelper licenseUsageHelper(systemContext);
            QnVirtualCameraResourceList cameras;

            licenseUsageHelper.propose(camera, param.scheduleEnabled);
            if (licenseUsageHelper.isOverflowForCamera(camera))
            {
                return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
                    "Set %1 `scheduleEnabled` to true is forbidden: no license to enable recording."),
                    param.cameraId));
            }
        }

        const auto enableBackupResult = checkIfCloudBackupMightBeEnabled(
            param, camera, systemContext);
        if (!enableBackupResult)
            return enableBackupResult;

        return Result();
    }

private:

    Result checkIfCloudBackupMightBeEnabled(
        const nx::vms::api::CameraAttributesData& param,
        const QnVirtualCameraResourcePtr& camera,
        SystemContext* systemContext) const
    {
        using namespace nx::vms::api;
        const bool isBackupTurnedOff = !camera || !camera->isBackupEnabled();
        const bool isParamEnablingBackupForDevice =
            param.backupPolicy == BackupPolicy::on
            || (param.backupPolicy == BackupPolicy::byDefault
                && systemContext->globalSettings()->backupSettings().backupNewCameras);

        const bool doesBackupGoFromOffToOn = isBackupTurnedOff && isParamEnablingBackupForDevice;
        const auto storages = systemContext->resourcePool()->getResources<QnStorageResource>();
        bool hasBackupCloudStorage = false;
        for (const auto& s: storages)
        {
            if (s->isBackup() && s->getStorageType() == nx::vms::api::kCloudStorageType)
            {
                hasBackupCloudStorage = true;
                break;
            }
        }

        NX_DEBUG(this,
            "%1: Checking if the backup status is going to be changed for the device "
            "and if it's permitted to do so. "
            "Backup turned off: %2. Backup is requested to be enabled: %3. "
            "Backup goes from off to on: %4. "
            "System has a backup cloud storage: %5",
            __func__, isBackupTurnedOff, isParamEnablingBackupForDevice, doesBackupGoFromOffToOn,
            hasBackupCloudStorage);

        const auto saasManager = systemContext->saasServiceManager();
        if (doesBackupGoFromOffToOn && hasBackupCloudStorage && saasManager->isEnabled())
        {
            const auto saasData = systemContext->saasServiceManager()->data();
            if (saasData.state != nx::vms::api::SaasState::active)
            {
                NX_DEBUG(
                    this, "%1: Can't enable cloud backup because "
                    "Saas service is not in the 'active' state", __func__);

                return Result(
                    ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
                        "It is forbidden to enable cloud backup for %1 "
                        "since SaaS service is not active."),
                    param.cameraId));
            }

            NX_DEBUG(this, "%1: Saas service is active");

            nx::vms::common::saas::CloudStorageServiceUsageHelper helper(systemContext);
            helper.proposeChange(
                /*devicesToAdd*/ {param.cameraId}, /*devicesToRemove*/ std::set<QnUuid>());
            if (helper.isOverflow())
            {
                NX_DEBUG(
                    this, "%1: Can't enable cloud backup because "
                    "there is no appropriate saas service license for the device %2",
                    __func__, param.cameraId);

                return Result(
                    ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
                        "It is forbidden to enable cloud backup for %1 "
                        "because there is no appropriate SaaS service license."),
                    param.cameraId));
            }

            NX_DEBUG(
                this, "%1: Cloud backup can be enabled for device %2",
                __func__, param.cameraId);
        }
        else
        {
            NX_DEBUG(this, "%1: Saas check is not needed. Backup can be enabled");
        }

        return Result();
    }
};

struct ModifyCameraAttributesListAccess
{
    Result operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::CameraAttributesDataList& param)
    {
        if (hasSystemAccess(accessData))
            return Result();

        for (const auto& p: param)
        {
            if (!checkSaveResourceAccess(systemContext, accessData, p.cameraId))
            {
                return Result(ErrorCode::forbidden, ServerApiErrors::tr(
                    "There is not enough save permissions"));
            }
        }

        using namespace nx::vms::license;
        CamLicenseUsageHelper licenseUsageHelper(systemContext);
        QnVirtualCameraResourceList cameras;

        const auto& resPool = systemContext->resourcePool();
        for (const auto& p: param)
        {
            auto camera = resPool->getResourceById(
                p.cameraId).dynamicCast<QnVirtualCameraResource>();
            if (!camera)
            {
                return Result(ErrorCode::forbidden,
                    nx::format(ServerApiErrors::tr("Device %1 not found"), p.cameraId));
            }
            cameras.push_back(camera);
            const bool prevScheduleEnabled = camera->isScheduleEnabled();
            if (prevScheduleEnabled != p.scheduleEnabled)
                licenseUsageHelper.propose(camera, p.scheduleEnabled);
        }

        for (const auto& camera : cameras)
            if (licenseUsageHelper.isOverflowForCamera(camera))
            {
                return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
                    "There is not enough license for camera %1"), camera->getId()));
            }
        return Result();
    }
};

struct ReadServerAttributesAccess
{
    Result operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const api::MediaServerUserAttributesData& param)
    {
        return checkReadResourceAccess(systemContext, accessData, param.serverId);
    }
};

struct ReadServerAttributesAccessOut
{
    RemotePeerAccess operator()(SystemContext* systemContext, const Qn::UserAccessData& accessData,
        const api::MediaServerUserAttributesData& param)
    {
        return checkReadResourceAccess(systemContext, accessData, param.serverId)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct ModifyServerAttributesAccess
{
    Result operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const api::MediaServerUserAttributesData& param)
    {
        if (hasSystemAccess(accessData))
            return Result();

        const auto& resPool = systemContext->resourcePool();
        auto accessManager = systemContext->resourceAccessManager();
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
            if (accessManager->hasPowerUserPermissions(accessData))
                return Result();
        }

        auto user = resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
        return accessManager->hasPermission(user, server, Qn::SavePermission)
            ? Result()
            : Result(ErrorCode::forbidden, ServerApiErrors::tr(
                "Saving Server attributes is forbidden: no saving permission."));
    }
};

static Result userHasGlobalAccess(
    SystemContext* systemContext,
    const Qn::UserAccessData& accessData,
    GlobalPermission permissions)
{
    if (hasSystemAccess(accessData))
        return Result();
    const auto resPool = systemContext->resourcePool();
    auto userResource = resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();
    if (!systemContext->resourceAccessManager()->hasGlobalPermission(userResource, permissions))
    {
        return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
            "User %1 with %2 permissions has no %3 permissions."),
            userResource ? userResource->getName() : accessData.userId.toString(),
            accessData.access,
            permissions));
    }

    return Result();
}

static Result userHasAccess(
    SystemContext* systemContext,
    const Qn::UserAccessData& accessData,
    const QnUuid& targetResourceOrGroupId,
    nx::vms::api::AccessRights requiredAccess)
{
    if (hasSystemAccess(accessData))
        return Result();
    const auto& resPool = systemContext->resourcePool();
    auto userResource = resPool->getResourceById(accessData.userId).dynamicCast<QnUserResource>();

    if (auto group = nx::vms::api::specialResourceGroup(targetResourceOrGroupId))
    {
        if (!systemContext->resourceAccessManager()->hasAccessRights(
            userResource, targetResourceOrGroupId, requiredAccess))
        {
            return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
                "User %1 does not have %2 access to %3."),
                userResource ? userResource->getName() : accessData.userId.toString(),
                requiredAccess, *group));
        }
    }
    else
    {
        auto targetResource = resPool->getResourceById(targetResourceOrGroupId);
        if (!targetResource)
        {
            return Result(ErrorCode::badRequest, nx::format(ServerApiErrors::tr(
                "Resource %1 does not exist."), targetResourceOrGroupId));
        }
        if (!systemContext->resourceAccessManager()->hasAccessRights(
            userResource, targetResource, requiredAccess))
        {
            return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
                "User %1 does not have %2 access to %3."),
                userResource ? userResource->getName() : accessData.userId.toString(),
                requiredAccess, targetResourceOrGroupId));
        }
    }

    return Result();
}

struct PowerUserAccess
{
    template<typename Param>
    Result operator()(
        SystemContext* systemContext, const Qn::UserAccessData& accessData, const Param&)
    {
        return userHasGlobalAccess(systemContext, accessData, GlobalPermission::powerUser);
    }
};

struct PowerUserAccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(
        SystemContext* systemContext, const Qn::UserAccessData& accessData, const Param&)
    {
        return userHasGlobalAccess(systemContext, accessData, GlobalPermission::powerUser)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

struct SaveUserRoleAccess
{
    Result operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::UserGroupData& param)
    {
        if (auto r = PowerUserAccess()(systemContext, accessData, param); !r)
        {
            r.message = ServerApiErrors::tr(
                "Saving Role is forbidden because the user has no admin access.");
            return r;
        }

        QSet<QnUuid> parentIds = nx::utils::toQSet(param.parentGroupIds);
        parentIds += systemContext->accessSubjectHierarchy()->recursiveParents(parentIds);

        if (parentIds.contains(param.id))
        {
            const auto cycledGroups = systemContext->accessSubjectHierarchy()->directMembers(
                param.id).intersect(parentIds);

            const auto cycledGroupNames = nx::vms::common::userGroupNames(
                systemContext, cycledGroups);

            return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
                "Circular dependencies are forbidden. This Role is already inherited by '%1'."),
                cycledGroupNames.join("', '")));
        }

        if (hasSystemAccess(accessData))
            return {};

        const auto existing = systemContext->userGroupManager()->find(param.id);

        if (param.type == nx::vms::api::UserType::ldap)
        {
            if (!existing)
            {
                return Result(ErrorCode::forbidden,
                    ServerApiErrors::tr("LDAP Group creation is forbidden."));
            }

            // TODO: (?) Is this change forbidden only for LDAP users?
            if (existing->externalId != param.externalId)
            {
                return Result(ErrorCode::badRequest,
                    ServerApiErrors::tr("Change of `externalId` is forbidden."));
            }
        }

        if (const auto res = canAddOrRemoveParentGroups(
                    systemContext->userGroupManager(),
                    accessData,
                    existing,
                    param.parentGroupIds);
            !res)
        {
            return res;
        }

        return {};
    }
};

struct RemoveUserRoleAccess
{
    Result operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::IdData& param)
    {
        if (auto r = PowerUserAccess()(systemContext, accessData, param); !r)
        {
            r.message = ServerApiErrors::tr(
                "Removing Role is forbidden because the user has no admin access.");
            return r;
        }

        const auto memberIds = systemContext->accessSubjectHierarchy()->directMembers(param.id);
        for (const auto& id: memberIds)
        {
            const auto member = systemContext->accessSubjectHierarchy()->subjectById(id);
            if (auto user = member.user())
            {
                return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
                    "Removing Role is forbidden because it is still used by '%1'."),
                    user->getName()));
            }
            else if (auto group = systemContext->userGroupManager()->find(id))
            {
                return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
                    "Removing Role is forbidden because it is still inherited by '%1'."),
                    group->name));
            }
        }

        return Result();
    }
};

struct VideoWallControlAccess
{
    Result operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::VideowallControlMessageData& data)
    {
        return userHasAccess(systemContext, accessData, data.videowallGuid,
            nx::vms::api::AccessRight::edit);
    }
};

struct ShowreelAccess
{
    Result operator()(
        SystemContext*,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::ShowreelData& showreel)
    {
        if (hasSystemAccess(accessData)
            || showreel.parentId.isNull()
            || accessData.userId == showreel.parentId)
        {
            return Result();
        }
        return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
            "User %1 is not allowed to modify the Showreel with parentId %2."),
            accessData.userId,
            showreel.parentId));
    }
};

struct ShowreelAccessById
{
    Result operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::IdData& showreelId)
    {
        const auto showreel = systemContext->showreelManager()->showreel(
            showreelId.id);
        if (!showreel.isValid())
            return Result(); //< Allow everyone to work with tours which are already deleted.
        return ShowreelAccess()(systemContext, accessData, showreel);
    }
};

struct InvalidFilterFunc
{
    template<typename ParamType>
    void operator()(SystemContext*, const Qn::UserAccessData&, ParamType&)
    {
        NX_ASSERT(false,
            "This transaction (%1) param type doesn't support filtering", typeid(ParamType));
    }
};

template<typename SingleAccess>
struct AccessOut
{
    template<typename Param>
    RemotePeerAccess operator()(
        SystemContext* systemContext, const Qn::UserAccessData& accessData, const Param& param)
    {
        return SingleAccess()(systemContext, accessData, param)
            ? RemotePeerAccess::Allowed
            : RemotePeerAccess::Forbidden;
    }
};

template<typename SingleAccess>
struct FilterListByAccess
{
    template<typename ParamContainer>
    void operator()(
        SystemContext* systemContext, const Qn::UserAccessData& accessData, ParamContainer& outList)
    {
        outList.erase(std::remove_if(
            outList.begin(),
            outList.end(),
            [&accessData, systemContext](auto& param) -> bool
            {
                return !SingleAccess()(systemContext, accessData, param);
            }),
            outList.end());
    }

    void operator()(
        SystemContext* systemContext, const Qn::UserAccessData& accessData, api::MediaServerDataExList& outList)
    {
        outList.erase(std::remove_if(
            outList.begin(),
            outList.end(),
            [&accessData, systemContext](const auto& param) -> bool
            {
                return !SingleAccess()(systemContext, accessData, param);
            }),
            outList.end());

        for (auto& i: outList)
            FilterListByAccess()(systemContext, accessData, i.storages);

    }
};

template<>
struct FilterListByAccess<ModifyResourceAccess>
{
    FilterListByAccess() {}

    template<typename ParamContainer>
    void operator()(
        SystemContext* systemContext, const Qn::UserAccessData& accessData, ParamContainer& outList)
    {
        outList.erase(std::remove_if(
            outList.begin(),
            outList.end(),
            [&accessData, systemContext](const auto& param) -> bool
            {
                return !ModifyResourceAccess()(systemContext, accessData, param);
            }),
            outList.end());
    }
};

template<>
struct FilterListByAccess<ModifyResourceParamAccess>
{
    FilterListByAccess(bool isRemove): isRemove(isRemove) {}

    void operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        nx::vms::api::ResourceParamWithRefDataList& outList)
    {
        outList.erase(std::remove_if(
            outList.begin(),
            outList.end(),
            [&accessData, systemContext, this](const auto& param) -> bool
            {
                return !ModifyResourceParamAccess(isRemove)(systemContext, accessData, param);
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
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const ParamContainer& paramContainer)
    {
        ParamContainer tmpContainer = paramContainer;
        FilterListByAccess<SingleAccess>()(systemContext, accessData, tmpContainer);
        if (paramContainer.size() != tmpContainer.size())
        {
            return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
                "Modifying of %1 entries out of %2 is not allowed for %3.", /*comment*/ "%3 is a C++ type name"),
                paramContainer.size() - tmpContainer.size(),
                paramContainer.size(),
                typeid(ParamContainer)));
        }
        return Result();
    }
};

template<typename SingleAccess>
struct ReadListAccess
{
    template<typename ParamContainer>
    bool operator()(SystemContext* systemContext, const Qn::UserAccessData& accessData, const ParamContainer& paramContainer)
    {
        ParamContainer tmpContainer = paramContainer;
        FilterListByAccess<SingleAccess>()(systemContext, accessData, tmpContainer);
        return tmpContainer.size() != paramContainer.size() && tmpContainer.empty();
    }
};

template<typename SingleAccess>
struct ReadListAccessOut
{
    template<typename ParamContainer>
    RemotePeerAccess operator()(SystemContext* systemContext, const Qn::UserAccessData& accessData, const ParamContainer& paramContainer)
    {
        ParamContainer tmpContainer = paramContainer;
        FilterListByAccess<SingleAccess>()(systemContext, accessData, tmpContainer);
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
    ec2::TransactionType operator()(SystemContext*, const Param&, AbstractPersistentStorage*)
    {
        return TransactionType::Regular;
    }
};

struct LocalTransactionType
{
    template<typename Param>
    ec2::TransactionType operator()(SystemContext*, const Param&, AbstractPersistentStorage*)
    {
        return TransactionType::Local;
    }
};

template <typename ParamsType>
struct SetStatusTransactionType
{
    ec2::TransactionType operator()(
        SystemContext* systemContext,
        const ParamsType& params,
        AbstractPersistentStorage* db)
    {
        const auto isServer =
            [resourcePool = systemContext->resourcePool(), db, &id = params.id]() -> bool
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
        filterByReadPermissionFunc, \
        checkRemotePeerAccessFunc, \
        getTransactionTypeFunc),

DescriptorBaseContainer transactionDescriptors = {
    TRANSACTION_DESCRIPTOR_LIST(TRANSACTION_DESCRIPTOR_APPLY)
};

#undef TRANSACTION_DESCRIPTOR_APPLY

} // namespace detail

namespace transaction_descriptor {

Result canModifyStorage(const CanModifyStorageData& data)
{
    if (data.modifyResourceResult != ErrorCode::ok)
        return data.modifyResourceResult;

    // For now only backup cloud storages are supported. It may change later.
    if (data.storageType == nx::vms::api::kCloudStorageType && !data.isBackup)
    {
        return Result(
            ErrorCode::forbidden,
            nx::format(
                detail::ServerApiErrors::tr(
                    "%1 storage can be only in the backup storage pool.",
                    /*comment*/ "%1 is the short cloud name (like Cloud)"),
                nx::branding::shortCloudName()));
    }

    if (!data.hasExistingStorage)
        return ErrorCode::ok;

    const auto existingStorage = data.getExistingStorageDataFunc();
    if (existingStorage.parentId == data.request.parentId
        && data.request.url != existingStorage.url)
    {
        data.logErrorFunc(nx::format(
            "Got inconsistent update request for storage '%1'. Urls differ.", data.request.id));
        return Result(
            ErrorCode::forbidden,
            detail::ServerApiErrors::tr("Changing storage URL is prohibited."));
    }

    return ErrorCode::ok;
}

} // namespace transaction_descriptor

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
