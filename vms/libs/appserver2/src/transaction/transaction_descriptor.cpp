// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "transaction_descriptor.h"

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

GlobalPermissions getGlobalPermissions(const SystemContext& context, const QnUuid& id)
{
    return context.resourceAccessManager()
        ->globalPermissions(context.accessSubjectHierarchy()->subjectById(id));
}

// Access and User info of the caller. Unlike `Qn::UserAccessData` can't represent 'Access::System'.
struct UserAccessorInfo
{
    QnUuid id;
    QString name;
    GlobalPermissions permissions;
};

auto userAccessorInfo(const SystemContext& context, const Qn::UserAccessData& access)
    -> std::pair<UserAccessorInfo, Result>
{
    const auto existingUser =
        context.resourcePool()->getResourceById<QnUserResource>(access.userId);

    if (!NX_ASSERT(existingUser))
    {
        NX_WARNING(NX_SCOPE_TAG, "User '%1' not found.", access.userId);
        return {{}, {ErrorCode::serverError}};
    }

    // TODO: Regardless of the mutex locks getting these properties is not atomic,
    //  hence a data race is possible.
    const QnUuid id = existingUser->getId();
    QString name = existingUser->getName();
    const GlobalPermissions permissions = getGlobalPermissions(context, id);

    return {UserAccessorInfo{.id = id, .name = std::move(name), .permissions = permissions}, {}};
}

// Returns the highest permission: `none` or `powerUser` or `administrator`.
GlobalPermission highestPermission(GlobalPermissions permissions)
{
    if (permissions.testFlag(GlobalPermission::administrator))
        return GlobalPermission::administrator;

    if (permissions.testFlag(GlobalPermission::powerUser))
        return GlobalPermission::powerUser;

    return GlobalPermission::none;
}

class CanAddOrRemoveParentGroups final
{
public:
    // Input type for User info.
    struct UserInfo
    {
        QnUuid id;
        QString name;
        std::vector<QnUuid> parentGroups;
    };

    // Input type for User Group info.
    struct UserGroupInfo
    {
        QnUuid id;
        QString name;
        api::UserType type;
        std::vector<QnUuid> parentGroups;
    };

    /**
     * Checks whether a new or existing User can be added to the given parent User Groups.
     * @param context
     * @param accessData
     * @param existingUser existing User. Value `nullptr` is considered as a new user to be
     * created.
     * @param parentGroups parent User Groups to add the User into
     * @return
     */
    [[nodiscard]] Result operator()(const SystemContext& context,
        const UserAccessorInfo& accessor,
        UserInfo user,
        std::vector<QnUuid> parentGroups) const
    {
        return canAddOrRemove(context, accessor, std::move(user), std::move(parentGroups));
    }

    /**
     * Checks whether a new or existing User Group can be added to the given parent User Groups.
     * @param context
     * @param accessData
     * @param existingGroup existing User Group. Value `nullopt` is considered as a new User Group
     * to be created.
     * @param parentGroups parent User Groups to add the User Group into
     * @return
     */
    [[nodiscard]] Result operator()(const SystemContext& context,
        const UserAccessorInfo& accessor,
        UserGroupInfo group,
        std::vector<QnUuid> parentGroups) const
    {
        return canAddOrRemove(context, accessor, std::move(group), std::move(parentGroups));
    }

private:
    struct SelectedGroupProps
    {
        QnUuid id;
        QString name;
        api::UserType type;
    };

    template<typename UserOrGroup>
    [[nodiscard]] Result canAddOrRemove(const nx::vms::common::SystemContext& context,
        const UserAccessorInfo& accessor,
        UserOrGroup child,
        std::vector<QnUuid> parentIds) const
    {
        static_assert(
            std::is_same_v<UserOrGroup, UserInfo> || std::is_same_v<UserOrGroup, UserGroupInfo>,
            "Unexpected resource type: only user or user group are allowed.");

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

        if constexpr (std::is_same_v<UserOrGroup, UserGroupInfo>)
        {
            if (Result r = checkForCircularDependencies(context, child, parentIds); !r)
                return r;
        }

        using nx::utils::unique_sorted;

        const auto existing = unique_sorted(std::move(child.parentGroups));
        const auto updated = unique_sorted(std::move(parentIds));

        const auto selectGroupProps =
            [](const api::UserGroupData& groupData)
            {
                return SelectedGroupProps{
                    .id = groupData.id, .name = groupData.name, .type = groupData.type};
            };

        const auto& groupManager = *context.userGroupManager();
        const auto [newParents, newNotFound] =
            groupManager.mapGroupsByIds(setDifference(updated, existing), selectGroupProps);

        if (!newNotFound.empty())
        {
            // TODO: (?) Can report all the missing groups.
            return {ErrorCode::badRequest,
                nx::format(ServerApiErrors::tr("User Group '%1' does not exist."),
                    newNotFound.front().toString())};
        }

        const auto [removeParents, removeNotFound] =
            groupManager.mapGroupsByIds(setDifference(existing, updated), selectGroupProps);

        for (const QnUuid& id: removeNotFound)
        {
            NX_WARNING(this,
                "User '%1(%2)' has requested to remove from non-existing User Group '%3'.",
                accessor.name,
                accessor.id.toString(),
                id);
        }

        const auto accessorPermissions = accessor.permissions;

        for (const auto& parent: newParents)
        {
            if (parent.type == api::UserType::ldap)
                return forbidAddToLdap(child, parent);

            if (parent.type != api::UserType::local)
            {
                return {ErrorCode::badRequest,
                    nx::format(
                        ServerApiErrors::tr("Cannot add to a non-local (%1) User Group '%2(%3)'."),
                        parent.type,
                        parent.name,
                        parent.id.toString())};
            }

            if (!accessorPermissions.testAnyFlags(
                GlobalPermission::powerUser | GlobalPermission::administrator))
            {
                return forbidAddToGroup(accessor, child, parent);
            }

            // This calls a dfs traversal with multiple mutex locks/unlocks.
            // Very far from being cache-friendly.
            const auto parentPermissions = getGlobalPermissions(context, parent.id);

            // TODO: Clarify this. Local Administrators are not allowed to create new Admins.
            // Quotes from the "Permission-clarification" table:
            // > (Creation) Any local user except Administrator and read-only users
            // > Also it is impossible to add any group into Administrators group.
            if (parentPermissions.testFlag(GlobalPermission::administrator))
                return forbidAddToGroup(accessor, child, parent);

            if (parentPermissions.testFlag(GlobalPermission::powerUser)
                && !accessorPermissions.testFlag(GlobalPermission::administrator))
            {
                return forbidAddToGroup(accessor, child, parent);
            }
        }

        for (const auto& parent: removeParents)
        {
            if (parent.type == api::UserType::ldap)
                return forbidRemoveFromLdap(child, parent);

            if (!NX_ASSERT(parent.type == api::UserType::local))
            {
                NX_DEBUG(this, "Unexpected parent User Group type '%1'.", parent.type);
                return {ErrorCode::serverError};
            }

            if (!accessorPermissions.testFlag(GlobalPermission::powerUser)
                && !accessorPermissions.testFlag(GlobalPermission::administrator))
            {
                return forbidRemoveFromGroup(accessor, child, parent);
            }

            // This calls a dfs traversal with multiple mutex locks/unlocks.
            // Very far from being cache-friendly.
            const auto parentPermissions = getGlobalPermissions(context, parent.id);

            // TODO: Clarify this. Local Administrators are not allowed to remove Admins.
            if (parentPermissions.testFlag(GlobalPermission::administrator))
                return forbidRemoveFromGroup(accessor, child, parent);

            if (parentPermissions.testFlag(GlobalPermission::powerUser)
                && !accessorPermissions.testFlag(GlobalPermission::administrator))
            {
                return forbidRemoveFromGroup(accessor, child, parent);
            }
        }

        return {};
    }

    static Result checkForCircularDependencies(const SystemContext& context,
        const UserGroupInfo& group,
        const std::vector<QnUuid>& parents)
    {
        QSet<QnUuid> parentIds = nx::utils::toQSet(parents);
        parentIds += context.accessSubjectHierarchy()->recursiveParents(parentIds);

        if (parentIds.contains(group.id) && group.type != nx::vms::api::UserType::ldap)
        {
            const auto cycledGroups =
                context.accessSubjectHierarchy()->directMembers(group.id).intersect(parentIds);

            const QStringList cycledGroupNames =
                [&]
                {
                    auto [selected, notFound] = context.userGroupManager()->mapGroupsByIds(cycledGroups,
                        [](const api::UserGroupData& group) -> QString
                        {
                            return NX_FMT("%1(%2)", group.name, group.id.toString());
                        });

                    return QStringList(std::make_move_iterator(selected.begin()),
                        std::make_move_iterator(selected.end()));
                }();

            return {ErrorCode::forbidden,
                nx::format(
                    ServerApiErrors::tr(
                        "Circular dependencies are forbidden. The User Group '%1(%2)' is already "
                        "inherited by '%3'."),
                    group.name,
                    group.id.toString(),
                    cycledGroupNames.join("', '"))};
        }

        return {};
    }

    template<typename Info>
    static Result forbidAddToLdap(
        const Info& /*userOrGroup*/, const SelectedGroupProps& group)
    {
        static const auto errorTemplate =
            []() -> QString
            {
                if constexpr (std::is_same_v<Info, UserInfo>)
                {
                    return ServerApiErrors::tr(
                        "Forbidden to add any Users to an LDAP User Group '%1(%2)'.");
                }
                else
                {
                    static_assert(std::is_same_v<Info, UserGroupInfo>,
                        "Unexpected resource type: only user or user group are allowed.");

                    return ServerApiErrors::tr(
                        "Forbidden to add any User Groups to an LDAP User Group '%1(%2)'.");
                }
            }();

        return {ErrorCode::forbidden, nx::format(errorTemplate, group.name, group.id.toString())};
    }

    template<typename Info>
    static Result forbidRemoveFromLdap(
        const Info& /*userOrGroup*/, const SelectedGroupProps& group)
    {
        static const auto errorTemplate =
            []() -> QString
            {
                if constexpr (std::is_same_v<Info, UserInfo>)
                {
                    return ServerApiErrors::tr(
                        "Forbidden to remove any Users from an LDAP User Group '%1(%2)'.");
                }
                else
                {
                    static_assert(std::is_same_v<Info, UserGroupInfo>,
                        "Unexpected resource type: only user or user group are allowed.");

                    return ServerApiErrors::tr(
                        "Forbidden to remove any User Groups from an LDAP User Group '%1(%2)'.");
                }
            }();

        return {ErrorCode::forbidden, nx::format(errorTemplate, group.name, group.id.toString())};
    }

    template<typename Info>
    static Result forbidAddToGroup(const UserAccessorInfo& accessor,
        const Info& userOrGroup,
        const SelectedGroupProps& parentGroup)
    {
        static const auto errorTemplate =
            []() -> QString
            {
                if constexpr (std::is_same_v<Info, UserInfo>)
                {
                    return ServerApiErrors::tr(
                        "User '%1(%2)' is not permitted to add the User '%3(%4)' to the User Group '%5(%6)'.");
                }
                else
                {
                    static_assert(std::is_same_v<Info, UserGroupInfo>,
                        "Unexpected resource type: only user or user group are allowed.");

                    return ServerApiErrors::tr(
                        "User '%1(%2)' is not permitted to add the User Group '%3(%4)' to the User Group '%5(%6)'.");
                }
            }();

        return {ErrorCode::forbidden,
            nx::format(errorTemplate,
                accessor.name,
                accessor.id,

                userOrGroup.name,
                userOrGroup.id,

                parentGroup.name,
                parentGroup.id)};
    }

    template<typename Info>
    static Result forbidRemoveFromGroup(const UserAccessorInfo& accessor,
        const Info& userOrGroup,
        const SelectedGroupProps& parentGroup)
    {
        static const auto errorTemplate =
            []() -> QString
            {
                if constexpr (std::is_same_v<Info, UserInfo>)
                {
                    return ServerApiErrors::tr(
                        "User '%1(%2)' is not permitted to remove the User '%3(%4)' "
                        "from the User Group '%5(%6)'.");
                }
                else
                {
                    static_assert(std::is_same_v<Info, UserGroupInfo>,
                        "Unexpected resource type: only user or user group are allowed.");

                    return ServerApiErrors::tr(
                        "User '%1(%2)' is not permitted to remove the User Group '%3(%4)' "
                        "from the User Group '%5(%6)'.");
                }
            }();

        return {ErrorCode::forbidden,
            nx::format(errorTemplate,
                accessor.name, accessor.id,
                userOrGroup.name, userOrGroup.id,
                parentGroup.name, parentGroup.id)};
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
        NX_CRITICAL(systemContext);


        if (hasSystemAccess(accessData))
            return ModifyResourceAccess()(systemContext, accessData, param);

        auto [accessor, accessorResult] = userAccessorInfo(*systemContext, accessData);
        if (!accessorResult)
            return std::move(accessorResult);

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

        // `QnUserResource` accessors may not provide consistent information (due to data races)
        // regardless of having mutexes.
        // Converting to `api::UserData` gives a chance to reduce the possibility of data races
        // (although not preventing it completely).
        const auto apiData =
            [](const QnUserResource& user)
            {
                api::UserData data;
                fromResourceToApi(user, data);
                return data;
            };

        if (Result r = !existingUser
                ? validateUserCreation(*systemContext, accessor, param)
                : validateUserModifications(
                    *systemContext, accessor, apiData(*existingUser), param);
            !r)
        {
            return r;
        }

        // TODO: This should be handled by `forbidNonLocalModification`.
        if (param.type == nx::vms::api::UserType::cloud
            && ((existingUser && param.fullName != existingUser->fullName())
                || (!existingUser && !param.fullName.isEmpty())))
        {
            return Result(ErrorCode::forbidden, nx::format(ServerApiErrors::tr(
                "User full name is controlled by the %1.", /*comment*/ "%1 is the short Cloud name"),
                nx::branding::shortCloudName()));
        }

        NX_ASSERT(resourcePool->getResourcesByIds<QnUserResource>(
            param.resourceAccessRights, [](const auto& pair) { return pair.first; }).empty(),
            "User should not be an accessible resource");

        return ModifyResourceAccess()(systemContext, accessData, param);
    }

    Result operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::UserDataDeprecated& param)
    {
        return (*this)(systemContext, accessData, param.toUserData());
    }

private:
    static Result validateUserCreation(const SystemContext& context,
        const UserAccessorInfo& accessor,
        const api::UserData& user)
    {
        if (!accessor.permissions.testAnyFlags(
            GlobalPermission::administrator | GlobalPermission::powerUser))
        {
            return forbidUserCreation(accessor);
        }

        if (user.attributes.testFlag(api::UserAttribute::readonly))
            return forbidReadonlyCreation();

        if (Result r = checkNonLocalCreation(user.type); !r)
            return r;

        if (Result r = checkAdHocCloudCreation(user); !r)
            return r;

        // TODO: #skolesnik
        //     Check for attempts to assign `administrator` or `powerUser` as custom rights.
        //     Example: new set of `parentIds` doesn't have a `powerUser` group, but `attributes`
        //         has the flag set. In this case a error should be issued stating that creating
        //         a `Power User` should be done via adding to a `Power Users` (or derived)
        //         User Group.
        //         If `parentIds` contain a power user group, but attribute 'powerUser' is not set,
        //         (???) no error should be reported.

        // TODO: #skolesnik
        //     Although it is not allowed to inherit from the `Administrators` group, it is
        //     still technically more correct to perform the hierarchy check.
        //     Also, `Administrator` as a "custom right" should also be checked (?not allowed).
        if (user.isAdministrator())
        {
            return {ErrorCode::forbidden,
                nx::format(ServerApiErrors::tr("Creating an Administrator user is not allowed."))};
        }

        // TODO: #skolesnik Clarify the usage of `canAddOrRemoveParentGroups` here. This will
        //     result in an ambiguous error "forbidden to add" instead of "forbidden to create".
        //     The error text should at least mention that the user is new: "forbidden to add _new_".
        using UserInfo = CanAddOrRemoveParentGroups::UserInfo;
        if (Result r = canAddOrRemoveParentGroups(context,
                accessor,
                UserInfo{.id = user.id, .name = user.name, .parentGroups = {}},
                user.groupIds);
            !r)
        {
            return r;
        }

        return {};
    }

    static Result validateUserModifications(const SystemContext& context,
        const UserAccessorInfo& accessor,
        const api::UserData& existing,
        const api::UserData& modified)
    {
        const bool isSelfModification = accessor.id == existing.id;

        if (Result r = !isSelfModification
                ? checkIfCanModifyUser(context, accessor, existing)
                : Result{};
            !r)
        {
            return r;
        }

        if (existing.attributes.testFlag(api::UserAttribute::readonly))
            return forbidReadonlyModification();

        const bool isLocal = existing.type == api::UserType::local
            || existing.type == api::UserType::temporaryLocal;

        if (existing.type != modified.type)
        {
            // This might happen for `v{1-2}` requests. For `v{3-}` `type` is expected to be
            // ignored by the schema.
            NX_WARNING(NX_SCOPE_TAG, "Attempt to modify UserDara `type` property, "
                                     "which is ignored by the OpenAPI schema.");

            return {ErrorCode::forbidden,
                nx::format(ServerApiErrors::tr("Forbidden to modify User's 'type' from '%1' to '%2'."),
                    existing.type,
                    modified.type)};
        }

        if (existing.type == nx::vms::api::UserType::temporaryLocal)
        {
            const auto temporaryToken =
                QJson::deserialized<nx::vms::api::TemporaryToken>(modified.hash);
            if (!temporaryToken.isValid())
                return {ErrorCode::forbidden, ServerApiErrors::tr("Invalid temporary token")};
        }

        if (isSelfModification && existing.isEnabled != modified.isEnabled)
        {
            const static QString errTemplate = existing.isEnabled
                ? "User '%1(%2)' can't disable himself."
                : "User '%1(%2)' can't enable himself.";

            return {ErrorCode::forbidden,
                nx::format(errTemplate, existing.name, existing.getId().toString())};
        }

        if (existing.name != modified.name)
        {
            if (!isLocal)
                return forbidNonLocalModification("name", existing);

            if (isSelfModification
                && (accessor.permissions.testFlag(GlobalPermission::administrator)
                    || accessor.permissions.testFlag(GlobalPermission::powerUser)))
            {
                return forbidSelfModification("name", existing);
            }
        }

        if (!isLocal && existing.fullName != modified.fullName)
            return forbidNonLocalModification("fullName", existing);

        if (!isLocal && existing.email != modified.email)
            return forbidNonLocalModification("email", existing);

        // TODO: (?) `permissions`

        if (existing.externalId != modified.externalId)
            return forbidExternalIdModification();

        // TODO: (?) `attributes`

        if (Result r = checkDigestModifications(existing, modified); !r)
            return r;

        // TODO: (?) hash
        // TODO: (?) cryptSha512Hash

        // TODO: #skolesnik
        //     Check for attempts to assign `administrator` or `powerUser` as custom rights.
        //     Example: new set of `parentIds` doesn't have a `powerUser` group, but `attributes`
        //         has the flag set. In this case a error should be issued stating that creating
        //         a `Power User` should be done via adding to a `Power Users` (or derived)
        //         User Group.
        //         If `parentIds` contain a power user group, but attribute 'powerUser' is not set,
        //         (???) no error should be reported.
        using UserInfo = CanAddOrRemoveParentGroups::UserInfo;
        if (const auto r = canAddOrRemoveParentGroups(context,
                accessor,
                UserInfo{
                    .id = existing.id, .name = existing.name, .parentGroups = existing.groupIds},
                modified.groupIds);
            !r)
        {
            return r;
        }

        return {};
    }

    static Result checkIfCanModifyUser(const SystemContext& context,
        const UserAccessorInfo& accessor,
        const api::UserData& existing)
    {
        // TODO: Provide this info with `existing` to reduce potential data races.
        const GlobalPermissions target = getGlobalPermissions(context, existing.id);

        if (target.testFlag(GlobalPermission::administrator))
        {
            return forbidUserModification(accessor, existing, GlobalPermission::administrator);
        }

        if (!accessor.permissions.testFlag(GlobalPermission::administrator)
            && !accessor.permissions.testFlag(GlobalPermission::powerUser))
        {
            return forbidUserModification(accessor, existing, GlobalPermission::none);
        }

        if (target.testFlag(GlobalPermission::powerUser)
            && !accessor.permissions.testFlag(GlobalPermission::administrator))
        {
            return forbidUserModification(accessor, existing, GlobalPermission::administrator);
        }

        return {};
    }

    static Result checkNonLocalCreation(api::UserType requestedType)
    {
        // Ad hoc to allow Cloud users creation via VMS API, although the API Documentations
        // explicitly says that only local users are allowed to be created via the API.
        // TODO: Disallow creation of Cloud users?
        if (requestedType == api::UserType::cloud)
            return {};

        if (requestedType != api::UserType::local
            && requestedType != api::UserType::temporaryLocal)
        {
            return {ErrorCode::forbidden,
                nx::format(
                    ServerApiErrors::tr("Creation of a non-local (%1) User is forbidden for VMS."),
                    requestedType)};
        }

        return {};
    }

    static Result checkDigestModifications(
        const api::UserData& existing, const api::UserData& modified)
    {
        // TODO: Simplify?
        if (modified.digest != nx::vms::api::UserData::kHttpIsDisabledStub
            && (modified.type == nx::vms::api::UserType::cloud
                || existing.digest == modified.digest)
            && existing.name != modified.name)
        {
            return {ErrorCode::forbidden,
                (modified.type == nx::vms::api::UserType::cloud)
                    ? nx::format(ServerApiErrors::tr(
                                     "User '%1' will not be saved because names differ: "
                                     "'%1' vs '%2' and changing name is forbidden for %3 users.",
                                     /*comment*/ "%3 is the short Cloud name"),
                        existing.name,
                        modified.name,
                        nx::branding::shortCloudName())
                    : nx::format(
                        ServerApiErrors::tr("User '%1' will not be saved because names differ: "
                                            "'%1' vs '%2' and `password` has not been provided."),
                        existing.name,
                        modified.name)};
        }

        return {};
    }

    static Result forbidSelfModification(const QString& propName, const api::UserData& user)
    {
        return {ErrorCode::forbidden,
            nx::format(ServerApiErrors::tr(
                           "User '%1(%2)' is not permitted to change his own property '%3'."),
                user.name,
                user.id.toString(),
                propName)};
    };

    static Result forbidReadonlyModification()
    {
        return {ErrorCode::forbidden,
            ServerApiErrors::tr(
                "Change of the User with readonly attribute is forbidden for VMS.")};
    }

    static Result forbidReadonlyCreation()
    {
        return {ErrorCode::forbidden,
            ServerApiErrors::tr(
                "Creation of the User with readonly attribute is forbidden for VMS.")};
    }

    static Result forbidExternalIdModification()
    {
        return {ErrorCode::forbidden, ServerApiErrors::tr("Change of `externalId` is forbidden.")};
    }

    static Result forbidNonLocalModification(const QString& propName, const api::UserData& user)
    {
        return {ErrorCode::forbidden,
            nx::format(ServerApiErrors::tr(
                           "Cannot modify property '%1' of a non-local (%2) User '%3(%4)'."),
                propName,
                user.type,
                user.name,
                user.id.toString())};
    }

    static Result forbidUserModification(const UserAccessorInfo& accessor,
        const api::UserData& target,
        GlobalPermission targetPermission)
    {
        return {ErrorCode::forbidden,
            nx::format(
                ServerApiErrors::tr(
                    "User '%1(%2)' is not permitted to modify User '%3(%4)' with '%5' rights."),
                accessor.name,
                accessor.id.toString(),
                target.name,
                target.id.toString(),
                targetPermission)};
    }

    static Result forbidUserCreation(const UserAccessorInfo& accessor)
    {
        return {ErrorCode::forbidden,
            nx::format(ServerApiErrors::tr("User '%1(%2)' is not permitted to create new Users."),
                accessor.name,
                accessor.id.toString())};
    }

    static Result forbidUserCreation(const UserAccessorInfo& accessor,
        const api::UserData& target,
        GlobalPermission targetPermission)
    {
        return {ErrorCode::forbidden,
            nx::format(ServerApiErrors::tr(
                           "User '%1(%2)' is not permitted to create a User with '%3' rights."),
                accessor.name,
                accessor.id.toString(),
                targetPermission)};
    }

    // Ad Hoc function to provide error messages when trying to create a Cloud User with values
    // that are supposed to be managed by the Cloud.
    // TODO: Just forbid Cloud User creation?
    static Result checkAdHocCloudCreation(const api::UserData& user)
    {
        if (user.type != api::UserType::cloud)
            return {};

        if (!user.fullName.isEmpty())
            return {ErrorCode::forbidden, "User full name is controlled by the Cloud."};

        return {};
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
        if (const auto r = checkReadResourceAccess(systemContext, accessData, param.resourceId); !r)
            return r;

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
        return checkReadResourceAccess(systemContext, accessData, param.resourceId)
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

class SaveServerDataAccess
{
public:
    Result operator()(SystemContext* context,
        const Qn::UserAccessData& accessData,
        const api::MediaServerData& param) const
    {
        NX_CRITICAL(context);

        if (hasSystemAccess(accessData))
            return {};

        auto [accessor, accessorResult] = userAccessorInfo(*context, accessData);
        if (!accessorResult)
            return std::move(accessorResult);

        if (!accessor.permissions.testAnyFlags(
                {GlobalPermission::administrator, GlobalPermission::powerUser}))
        {
            return forbidSettingsModification(accessor);
        }

        const auto existing =
            context->resourcePool()->getResourceById<QnMediaServerResource>(param.id);

        if (!existing)
        {
            if (!accessor.permissions.testFlag(GlobalPermission::administrator))
            {
                return {ErrorCode::forbidden,
                    nx::format(
                        ServerApiErrors::tr(
                            "User '%1(%2)' is not permitted to create a new Server."),
                        accessor.name,
                        accessor.id.toString())};
            }
        }

        return {};
    }

private:
    template<typename User>
    static Result forbidSettingsModification(const User& user)
    {
        return {ErrorCode::forbidden,
            nx::format(ServerApiErrors::tr(
                           "User '%1(%2)' is not permitted to modify the Server's settings."),
                user.name,
                user.id.toString())};
    }
};

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
        NX_CRITICAL(systemContext);

        if (hasSystemAccess(accessData))
            return {};

        auto [accessor, accessorResult] = userAccessorInfo(*systemContext, accessData);
        if (!accessorResult)
            return std::move(accessorResult);

        const auto existing = systemContext->userGroupManager()->find(param.id);

        if (existing)
        {
            // TODO: #skolesnik (?) Should we also forbid modification for `systemAccess`?
            if (Result r = checkForBuiltInModification(*existing); !r)
                return r;

            if (Result r = checkIfCanModifyUserGroup(*systemContext, accessor, *existing); !r)
                return r;

            if (existing->externalId != param.externalId)
            {
                return {ErrorCode::badRequest,
                    ServerApiErrors::tr("Change of `externalId` is forbidden.")};
            }
        }
        else
        {
            if (!accessor.permissions.testAnyFlags(
                {GlobalPermission::administrator, GlobalPermission::powerUser}))
            {
                return {ErrorCode::forbidden,
                    nx::format(
                        ServerApiErrors::tr(
                            "User '%1(%2) is not permitted to create new User Groups.'"),
                        accessor.name,
                        accessor.id)};
            }

            if (Result r = checkNonLocalCreation(param.type); !r)
                return r;
        }

        // `forbidUserGroupModification` will most likely shadow the removal errors...
        // TODO: #skolesnik Clarify the usage of `canAddOrRemoveParentGroups` here. This will
        //     result in an ambiguous error "forbidden to add" instead of "forbidden to create".
        //     The error text should at least mention that the user group is new: "forbidden to add _new_".
        using UserGroupInfo = CanAddOrRemoveParentGroups::UserGroupInfo;
        if (const auto r = canAddOrRemoveParentGroups(*systemContext,
                accessor,
                UserGroupInfo{.id = existing ? existing->id : param.id,
                    .name = existing ? existing->name : param.name,
                    .type = existing ? existing->type : param.type,
                    .parentGroups = existing ? existing->parentGroupIds : std::vector<QnUuid>()},
                param.parentGroupIds);
            !r)
        {
            return r;
        }

        NX_ASSERT(systemContext->resourcePool()->getResourcesByIds<QnUserResource>(
            param.resourceAccessRights, [](const auto& pair) { return pair.first; }).empty(),
            "User should not be an accessible resource");

        return {};
    }

private:
    static Result checkIfCanModifyUserGroup(const SystemContext& context,
        const UserAccessorInfo& accessor,
        const api::UserGroupData& existing)
    {
        // TODO: Provide this info with `existing` to reduce potential data races.
        const GlobalPermission target =
            static_cast<GlobalPermission>(getGlobalPermissions(context, existing.id).toInt());

        const bool isTargetAdmin = target & GlobalPermission::administrator;
        const bool isTargetPowerUser = target & GlobalPermission::powerUser;
        const bool isAccessorAdmin = accessor.permissions & GlobalPermission::administrator;
        const bool isAccessorPowerUser = accessor.permissions & GlobalPermission::powerUser;

        if (isTargetAdmin
            || (!isAccessorAdmin && !isAccessorPowerUser)
            || (isTargetPowerUser && !isAccessorAdmin))
        {
            return forbidUserGroupModification(accessor, existing, target);
        }

        return {};
    }

    static Result checkNonLocalCreation(api::UserType requestedType)
    {
        if (requestedType != api::UserType::local
            && requestedType != api::UserType::temporaryLocal)
        {
            return {ErrorCode::forbidden,
                nx::format(
                    ServerApiErrors::tr("Creation of a non-local (%1) User Group is forbidden for VMS."),
                    requestedType)};
        }

        return {};
    }

    static Result checkForBuiltInModification(const api::UserGroupData& existingGroup)
    {
        if (!api::kPredefinedGroupIds.contains(existingGroup.id))
            return {};

        return {ErrorCode::forbidden,
            nx::format(ServerApiErrors::tr("Cannot modify the built-in User Group %1(%2)."),
                existingGroup.name,
                existingGroup.id.toString())};
    }

    static Result forbidUserGroupModification(const UserAccessorInfo& accessor,
        const api::UserGroupData& target,
        GlobalPermission targetPermission)
    {
        return {ErrorCode::forbidden,
            nx::format(
                ServerApiErrors::tr(
                    "User '%1(%2)' is not permitted to modify User Group '%3(%4)' with '%5' rights."),
                accessor.name,
                accessor.id.toString(),
                target.name,
                target.id.toString(),
                highestPermission(targetPermission))};
    }
};

struct RemoveUserRoleAccess
{
    Result operator()(
        SystemContext* systemContext,
        const Qn::UserAccessData& accessData,
        const nx::vms::api::IdData& param)
    {
        if (hasSystemAccess(accessData))
            return {};

        NX_CRITICAL(systemContext);

        auto [accessor, accessorResult] = userAccessorInfo(*systemContext, accessData);
        if (!accessorResult)
            return std::move(accessorResult);

        if (Result r = expectPowerUserOrAdmin(accessor); !r)
            return r;

        const auto group = systemContext->userGroupManager()->find(param.id);
        if (!group)
        {
            return {ErrorCode::notFound,
                nx::format(
                    ServerApiErrors::tr("User Group '%1' does not exist."), param.id.toString())};
        }

        if (Result r = checkPermissionsToDelete(*systemContext, accessor, *group); !r)
            return r;

        if (Result r = checkGroupNotReferenced(*systemContext, *group); !r)
            return r;

        return {};
    }

private:
    // General assessment that can and should be performed before figuring whether the User Group
    // actually exist. We don't want to give up information about the group's existence to users
    // with no rights.
    static Result expectPowerUserOrAdmin(const UserAccessorInfo& accessor)
    {
        if (accessor.permissions & (GlobalPermission::powerUser | GlobalPermission::administrator))
            return {};

        return {ErrorCode::forbidden,
            nx::format(
                ServerApiErrors::tr("User '%1(%2)' is not permitted to delete User Groups."),
                accessor.name,
                accessor.id.toString())};
    }

    // Check whether the accessor has sufficient rights to delete the User Group.
    // This function assumes that the group exists.
    template<typename GroupInfo>
    static Result checkPermissionsToDelete(
        const SystemContext& context, const UserAccessorInfo& accessor, const GroupInfo& group)
    {
        const auto errorForbidden =
            [](const auto& user, const auto& group, GlobalPermission groupPermissions) -> Result
            {
                return {ErrorCode::forbidden,
                    nx::format(
                        ServerApiErrors::tr("User '%1(%2)' is not permitted to delete the "
                            "User Group '%3(%4)' with '%5' rights."),
                        user.name,
                        user.id.toString(),
                        group.name,
                        group.id.toString(),
                        highestPermission(groupPermissions))};
        };

        const auto groupPermissions =
            static_cast<GlobalPermission>(getGlobalPermissions(context, group.id).toInt());

        // As of this writing, deleting administrator groups is not permitted.
        if (groupPermissions & GlobalPermission::administrator)
            return errorForbidden(accessor, group, groupPermissions);

        if (!expectPowerUserOrAdmin(accessor))
            return errorForbidden(accessor, group, groupPermissions);

        if ((groupPermissions & GlobalPermission::powerUser)
            && !(accessor.permissions & GlobalPermission::administrator))
        {
            return errorForbidden(accessor, group, groupPermissions);
        }

        return {};
    }

    template<typename GroupInfo>
    static Result checkGroupNotReferenced(const SystemContext& context, const GroupInfo& userGroup)
    {
        // The Current group will not be removed from `parentIds` or `parentGroupIds` leaving
        // existing Users or User Groups with garbage ids of non-existing parents.
        // This is acceptable for LDAP groups because continuous sync is supposed to fix
        // such errors on the fly.
        if (userGroup.type == nx::vms::api::UserType::ldap)
            return {};

        // The hierarchy data is likely to change between iterations. Data race is possible.
        const auto memberIds = context.accessSubjectHierarchy()->directMembers(userGroup.id);
        for (const auto& id: memberIds)
        {
            // TODO: #skolesnik Informative error string referencing name and id.

            const auto member = context.accessSubjectHierarchy()->subjectById(id);
            if (auto user = member.user())
            {
                return {ErrorCode::forbidden,
                    nx::format(
                        ServerApiErrors::tr(
                            "Removing User Group is forbidden because it is still used by '%1'."),
                        user->getName())};
            }

            if (auto group = context.userGroupManager()->find(id))
            {
                return {ErrorCode::forbidden,
                    nx::format(
                        ServerApiErrors::tr(
                            "Removing User Group is forbidden because it is still inherited by '%1'."),
                        group->name)};
            }
        }

        return {};
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
