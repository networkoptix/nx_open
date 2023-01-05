// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_access_manager.h"

#include <algorithm>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/access_rights_resolver.h>
#include <core/resource_access/global_permissions_watcher.h>
#include <core/resource_access/permissions_cache.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/resource_access_subjects_cache.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/core/access/access_types.h>
#include <nx/fusion/model_functions.h>
#include <nx/streaming/abstract_archive_resource.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/api/data/analytics_data.h>
#include <nx/vms/api/data/camera_data.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/api/data/videowall_data.h>
#include <nx/vms/api/data/webpage_data.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/system_context.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <utils/common/scoped_timer.h>

using namespace nx::core::access;
using namespace nx::vms::api;
using namespace nx::vms::common;

namespace {

/** Check if there is a desktop camera on a layout and if it is allowed to be there. */
bool verifyDesktopCameraOnLayout(
    QnResourcePool* resourcePool,
    const QnResourceAccessSubject& subject,
    const nx::vms::api::LayoutData& data)
{
    // Find desktop camera on the layout.
    QnVirtualCameraResourcePtr desktopCamera;
    for (const auto& item: data.items)
    {
        nx::vms::common::ResourceDescriptor descriptor{item.resourceId, item.resourcePath};
        auto camera = resourcePool->getResourceByDescriptor(descriptor)
            .dynamicCast<QnVirtualCameraResource>();

        if (camera && camera->hasFlags(Qn::desktop_camera))
        {
            desktopCamera = camera;
            break;
        }
    }

    // It's ok if there is no desktop cameras.
    if (!desktopCamera)
        return true;

    // It is not allowed to create layouts with desktop camera and any other element.
    if (data.items.size() > 1)
        return false;

    // Only users can push their screen.
    if (!subject.user())
        return false;

    // User can push only own screen (check by uuid).
    if (!desktopCamera->getPhysicalId().endsWith(subject.user()->getId().toString()))
        return false;

    // User can push only own screen (check by name).
    if (desktopCamera->getName() != subject.user()->getName())
        return false;

    // Screen can be pushed only on videowall.
    if (!resourcePool->getResourceById(data.parentId).dynamicCast<QnVideoWallResource>())
        return false;

    return true;
}

} // namespace

QnResourceAccessManager::QnResourceAccessManager(
    nx::vms::common::SystemContext* context,
    QObject* parent)
    :
    base_type(parent),
    nx::vms::common::SystemContextAware(context),
    m_accessRightsResolver(new AccessRightsResolver(
        resourcePool(),
        systemContext()->accessRightsManager(),
        systemContext()->globalPermissionsWatcher(),
        systemContext()->accessSubjectHierarchy()))
{
    connect(m_accessRightsResolver.get(), &AccessRightsResolver::resourceAccessReset,
        this, &QnResourceAccessManager::resourceAccessReset, Qt::DirectConnection);
}

bool QnResourceAccessManager::hasAdminPermissions(const Qn::UserAccessData& accessData) const
{
    if (accessData == Qn::kSystemAccess)
        return true;

    return m_accessRightsResolver->hasAdminAccessRights(accessData.userId);
}

bool QnResourceAccessManager::hasAdminPermissions(const QnResourceAccessSubject& subject) const
{
    return subject.isValid() && (subject.isRole() || subject.user()->isEnabled())
        ? m_accessRightsResolver->hasAdminAccessRights(subject.id())
        : false;
}

AccessRights QnResourceAccessManager::accessRights(
    const QnResourceAccessSubject& subject, const QnResourcePtr& resource) const
{
    return subject.isValid() && (subject.isRole() || subject.user()->isEnabled())
        ? m_accessRightsResolver->accessRights(subject.id(), resource)
        : AccessRights{};
}

bool QnResourceAccessManager::hasAccessRights(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource, AccessRights requiredAccessRights) const
{
    return requiredAccessRights == AccessRights{}
        || (accessRights(subject, resource) & requiredAccessRights) == requiredAccessRights;
}

AccessRights QnResourceAccessManager::commonAccessRights(
    const QnResourceAccessSubject& subject) const
{
    return subject.isValid() && (subject.isRole() || subject.user()->isEnabled())
        ? m_accessRightsResolver->commonAccessRights(subject.id())
        : AccessRights{};
}

GlobalPermissions QnResourceAccessManager::globalPermissions(
    const QnResourceAccessSubject& subject) const
{
    if (!subject.isValid())
        return {};

    if (const auto user = subject.user())
    {
        if (!user->isEnabled())
            return {};

        if (!user->flags().testFlag(Qn::local) && !user->resourcePool())
            return {};
    }

    auto permissions = m_accessRightsResolver->globalPermissions(subject.id());

    // TODO: #vkutin Remove when transition to the new access rights is over.
    if (permissions.testFlag(GlobalPermission::admin))
        permissions |= GlobalPermission::adminPermissions;

    return permissions;
}

bool QnResourceAccessManager::hasGlobalPermission(
    const Qn::UserAccessData& accessData,
    GlobalPermission requiredPermission) const
{
    if (accessData == Qn::kSystemAccess)
        return true;

    const auto user = resourcePool()->getResourceById<QnUserResource>(accessData.userId);
    if (!user)
    {
        NX_VERBOSE(this, "User %1 is not found", accessData.userId);
        return false;
    }

    return hasGlobalPermission(user, requiredPermission);
}

bool QnResourceAccessManager::hasGlobalPermission(
    const QnResourceAccessSubject& subject,
    GlobalPermission requiredPermission) const
{
    return requiredPermission == GlobalPermission::none
        || globalPermissions(subject).testFlag(requiredPermission);
}

Qn::Permissions QnResourceAccessManager::permissions(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    if (!subject.isValid() || !resource)
        return Qn::NoPermissions;

    // User is already removed.
    if (const auto& user = subject.user())
    {
        if (!user->resourcePool() || user->hasFlags(Qn::removed))
            return Qn::NoPermissions;
    }

    // Resource is already removed.
    if (resource->hasFlags(Qn::removed))
        return Qn::NoPermissions;

    // Resource is not added to pool, checking if we can create such resource.
    if (!resource->resourcePool())
    {
        const auto result = canCreateResourceInternal(subject, resource)
            ? Qn::ReadWriteSavePermission
            : Qn::NoPermissions;
        NX_VERBOSE(this, "Permissions for %1 to create new %2 is %3", subject, resource, result);
        return result;
    }

    const auto result = calculatePermissions(subject, resource);
    NX_VERBOSE(this, "Calculated permissions for %1 ower %2 is %3", subject, resource, result);
    return result;
}

bool QnResourceAccessManager::hasPermission(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource,
    Qn::Permissions requiredPermissions) const
{
    return (permissions(subject, resource) & requiredPermissions) == requiredPermissions;
}

bool QnResourceAccessManager::hasPermission(
    const Qn::UserAccessData& accessRights,
    const QnResourcePtr& resource,
    Qn::Permissions permissions) const
{
    if (accessRights == Qn::kSystemAccess)
        return true;

    if (accessRights.access == Qn::UserAccessData::Access::ReadAllResources
        && permissions == Qn::ReadPermission)
    {
        return true;
    }

    auto userResource = resourcePool()->getResourceById<QnUserResource>(accessRights.userId);
    if (!userResource)
        return false;

    return hasPermission(userResource, resource, permissions);
}

QnResourceAccessManager::Notifier* QnResourceAccessManager::createNotifier(QObject* parent)
{
    return m_accessRightsResolver->createNotifier(parent);
}

bool QnResourceAccessManager::canCreateResourceInternal(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& target) const
{
    NX_ASSERT(subject.isValid());
    NX_ASSERT(target);
    NX_ASSERT(!isUpdating());

    if (!subject.isValid())
        return false;

    // Check new layouts creating.
    if (QnLayoutResourcePtr layout = target.dynamicCast<QnLayoutResource>())
        return canCreateLayout(subject, layout);

    // Check new users creating.
    if (const auto targetUser = target.dynamicCast<QnUserResource>())
    {
        return canCreateUser(subject,
            targetUser->getRawPermissions(),
            targetUser->userRoleIds(),
            targetUser->isOwner());
    }

    if (const auto storage = target.dynamicCast<QnStorageResource>())
        return canCreateStorage(subject, storage->getParentId());

    if (const auto videoWall = target.dynamicCast<QnVideoWallResource>())
        return canCreateVideoWall(subject);

    if (const auto webPage = target.dynamicCast<QnWebPageResource>())
        return canCreateWebPage(subject);

    return hasAdminPermissions(subject);
}

Qn::Permissions QnResourceAccessManager::calculatePermissions(
    const QnResourceAccessSubject& subject, const QnResourcePtr& target) const
{
    NX_ASSERT(target);
    if (subject.user() && !subject.user()->isEnabled())
        return Qn::NoPermissions;

    if (!target || !target->resourcePool())
        return Qn::NoPermissions;

    if (const auto targetUser = target.objectCast<QnUserResource>())
        return calculatePermissionsInternal(subject, targetUser);

    if (const auto targetLayout = target.objectCast<QnLayoutResource>())
        return calculatePermissionsInternal(subject, targetLayout);

    if (const auto targetServer = target.objectCast<QnMediaServerResource>())
        return calculatePermissionsInternal(subject, targetServer);

    if (const auto targetVideowall = target.objectCast<QnVideoWallResource>())
        return calculatePermissionsInternal(subject, targetVideowall);

    if (const auto targetWebPage = target.objectCast<QnWebPageResource>())
        return calculatePermissionsInternal(subject, targetWebPage);

    if (const auto targetCamera = target.objectCast<QnVirtualCameraResource>())
        return calculatePermissionsInternal(subject, targetCamera);

    if (const auto targetStorage = target.objectCast<QnStorageResource>())
        return calculatePermissionsInternal(subject, targetStorage);

    if (const auto targetArchive = target.objectCast<QnAbstractArchiveResource>())
        return Qn::ReadPermission | Qn::ExportPermission;

    if (const auto targetPlugin = target.objectCast<AnalyticsPluginResource>())
        return Qn::ReadPermission;

    if (const auto targetEngine = target.objectCast<AnalyticsEngineResource>())
        return Qn::ReadPermission;

    NX_ASSERT(false, "invalid resource type");
    return Qn::NoPermissions;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnVirtualCameraResourcePtr& camera) const
{
    Qn::Permissions result = Qn::NoPermissions;

    // Admins must be able to remove any cameras to delete servers.
    if (hasAdminPermissions(subject))
        result |= Qn::RemovePermission;

    const auto accessRights = this->accessRights(subject, camera);
    if (!accessRights.testFlag(AccessRight::view))
        return result;

    result |= Qn::ReadPermission | Qn::ViewContentPermission;

    const bool isLiveAllowed = !camera->needsToChangeDefaultPassword();
    bool isFootageAllowed = accessRights.testFlag(AccessRight::viewArchive);
    bool isExportAllowed = isFootageAllowed
        && accessRights.testFlag(AccessRight::exportArchive);

    if (!camera->isScheduleEnabled() && camera->isDtsBased())
    {
        switch (camera->licenseType())
        {
            case Qn::LC_Bridge:
            {
                isFootageAllowed = false;
                isExportAllowed = false;
                break;
            }

            // TODO: Forbid all for VMAX when discussed with management
            case Qn::LC_VMAX:
            {
                // isLiveAllowed = false;
                // footageAllowed = false;
                break;
            }
            default:
                break;
        }
    }

    if (isLiveAllowed)
        result |= Qn::ViewLivePermission;

    if (isFootageAllowed)
        result |= Qn::ViewFootagePermission;

    if (isExportAllowed)
    {
        NX_ASSERT(isFootageAllowed, "Server API cannot allow export without footage access.");
        result |= Qn::ExportPermission;
    }

    if (accessRights.testFlag(AccessRight::viewBookmarks))
    {
        result |= Qn::ViewBookmarksPermission;

        if (accessRights.testFlag(AccessRight::manageBookmarks))
            result |= Qn::ManageBookmarksPermission;
    }

    if (accessRights.testFlag(AccessRight::userInput))
    {
        result |= Qn::WritePtzPermission
            | Qn::DeviceInputPermission
            | Qn::SoftTriggerPermission
            | Qn::TwoWayAudioPermission;
    }

    if (accessRights.testFlag(AccessRight::edit))
        result |= Qn::ReadWriteSavePermission | Qn::WriteNamePermission;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnMediaServerResourcePtr& server) const
{
    Qn::Permissions result = Qn::ReadPermission;
    if (!hasAccessRights(subject, server, AccessRight::view))
        return result;

    result |= Qn::ViewContentPermission;

    if (hasAdminPermissions(subject))
        result |= Qn::FullServerPermissions;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnStorageResourcePtr& storage) const
{
    const auto parentServer = storage->getParentServer();

    //< Cloud storage isn't tied to any server and is readable for all.
    if (!parentServer)
    {
        if (hasAdminPermissions(subject))
            return Qn::ReadWriteSavePermission;

        return Qn::ReadPermission;
    }

    const auto serverPermissions = permissions(subject, parentServer);

    if (serverPermissions.testFlag(Qn::RemovePermission))
        return Qn::ReadWriteSavePermission | Qn::RemovePermission;

    if (serverPermissions.testFlag(Qn::SavePermission))
        return Qn::ReadWriteSavePermission;

    return Qn::NoPermissions;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnVideoWallResourcePtr& videoWall) const
{
    if (!hasAccessRights(subject, videoWall, AccessRight::controlVideowall))
        return Qn::NoPermissions;

    Qn::Permissions result =
          Qn::ReadPermission
        | Qn::WritePermission
        | Qn::SavePermission
        | Qn::WriteNamePermission;

    if (hasAdminPermissions(subject))
        result |= Qn::RemovePermission;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnWebPageResourcePtr& webPage) const
{
    const auto accessRights = this->accessRights(subject, webPage);
    if (!accessRights.testFlag(AccessRight::view))
        return Qn::NoPermissions;

    Qn::Permissions result = Qn::ReadPermission | Qn::ViewContentPermission;

    // Web Page behaves totally like camera.
    if (accessRights.testFlag(AccessRight::edit))
        result |= Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnLayoutResourcePtr& layout) const
{
    if (!subject.isValid())
        return Qn::NoPermissions;

    // TODO: #sivanov Code duplication with the same QnWorkbenchAccessController method.

    const auto checkLocked =
        [this, layout](const Qn::Permissions permissions)
        {
            if (!layout->locked())
                return permissions;

            Qn::Permissions forbidden = Qn::AddRemoveItemsPermission
                | Qn::WriteNamePermission
                | Qn::WritePermission;

            // Autogenerated layouts (e.g. videowall) can be removed even when locked.
            if (!layout->isServiceLayout())
                forbidden |= Qn::RemovePermission;

            return permissions & ~forbidden;
        };

    // Layouts with desktop cameras are not to be modified but can be removed.
    const auto items = layout->getItems();
    for (auto item: items)
    {
        if (const auto& resource = resourcePool()->getResourceByDescriptor(item.resource))
        {
            if (resource->hasFlags(Qn::desktop_camera))
                return Qn::ReadPermission | Qn::RemovePermission;
        }
    }

    const auto accessRights = this->accessRights(subject, layout);

    // Calculate base layout permissions.
    const auto base =
        [&]() -> Qn::Permissions
        {
            // Owner can do anything.
            if (subject.user() && subject.user()->isOwner())
                return Qn::FullLayoutPermissions;

            // Access to global layouts.
            // Simple check is enough, exported layouts are checked on the client side.
            if (layout->isShared())
            {
                if (!accessRights.testFlag(AccessRight::view))
                    return Qn::NoPermissions;

                // Global layouts editor.
                if (hasAdminPermissions(subject))
                    return Qn::FullLayoutPermissions;

                return Qn::ModifyLayoutPermission;
            }

            const auto ownerId = layout->getParentId();

            // Access to videowall layouts.
            const auto videowall = resourcePool()->getResourceById<QnVideoWallResource>(ownerId);
            if (videowall)
            {
                // Videowall layout.
                // TODO: #vkutin
                // We shouldn't need to do this check.
                return hasAccessRights(subject, videowall, AccessRight::controlVideowall)
                    ? Qn::FullLayoutPermissions
                    : Qn::NoPermissions;
            }

            // Access to intercom layouts.
            const auto camera = resourcePool()->getResourceById<QnResource>(ownerId);
            if (nx::vms::common::isIntercom(camera))
            {
                if (hasAccessRights(subject, camera, AccessRight::userInput))
                    return Qn::FullLayoutPermissions;

                return Qn::ReadPermission;
            }

            // Checking other user's layout. Only users can have access to them.
            if (const auto user = subject.user())
            {
                if (ownerId != user->getId())
                {
                    const auto owner = resourcePool()->getResourceById<QnUserResource>(ownerId);
                    if (!owner)
                    {
                        const auto tour = m_context->showreelManager()->tour(ownerId);
                        if (tour.isValid())
                        {
                            return tour.parentId == user->getId()
                                ? Qn::FullLayoutPermissions
                                : Qn::NoPermissions;
                        }

                        // Layout of user, which we don't know of.
                        return hasAdminPermissions(subject)
                            ? Qn::FullLayoutPermissions
                            : Qn::NoPermissions;
                    }

                    // We can modify layout for user if we can modify this user.
                    Qn::Permissions userPermissions = permissions(subject, owner);
                    if (userPermissions.testFlag(Qn::SavePermission))
                        return Qn::FullLayoutPermissions;

                    // We can see layouts for another users if we are able to see these users.
                    if (userPermissions.testFlag(Qn::ReadPermission))
                        return Qn::ModifyLayoutPermission;

                    return Qn::NoPermissions;
                }
            }

            // User can do whatever he wants with own layouts.
            return Qn::FullLayoutPermissions;
        };

    return checkLocked(base());
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnUserResourcePtr& targetUser) const
{
    const auto checkUserType =
        [targetUser](Qn::Permissions permissions)
        {
            switch (targetUser->userType())
            {
                case nx::vms::api::UserType::ldap:
                {
                    return permissions & ~(
                        Qn::WriteNamePermission
                        | Qn::WritePasswordPermission
                        | Qn::WriteEmailPermission);
                }
                case nx::vms::api::UserType::cloud:
                {
                    return permissions & ~(
                        Qn::WritePasswordPermission
                        | Qn::WriteDigestPermission
                        | Qn::WriteEmailPermission
                        | Qn::WriteFullNamePermission);
                }
                default:
                {
                    return permissions;
                }
            }
        };

    Qn::Permissions result = Qn::NoPermissions;
    const bool isSubjectOwner = subject.user() && subject.user()->isOwner();

    if (targetUser == subject.user())
    {
        /* Everyone can edit own data. */
        result |= Qn::ReadWriteSavePermission | Qn::WritePasswordPermission
            | Qn::WriteEmailPermission | Qn::WriteFullNamePermission;

        if (isSubjectOwner)
            result |= Qn::WriteDigestPermission;
    }
    else if (hasAdminPermissions(subject))
    {
        result |= Qn::ReadPermission;

        // Nobody can do anything with the owner (except digest mode change).
        if (targetUser->isOwner())
        {
            if (isSubjectOwner && !targetUser->isCloud())
                result |= Qn::ReadWriteSavePermission | Qn::WriteDigestPermission;

            return result;
        }

        // Admins can only be edited by owner, other users - by all admins.
        if (isSubjectOwner || !m_accessRightsResolver->hasAdminAccessRights(targetUser->getId()))
            result |= Qn::FullUserPermissions;
    }

    return checkUserType(result);
}

//-------------------------------------------------------------------------------------------------
// Layout

bool QnResourceAccessManager::canCreateLayout(
    const QnResourceAccessSubject& subject,
    const nx::vms::api::LayoutData& data) const
{
    if (!subject.isValid())
        return false;

    const auto& resourcePool = this->resourcePool();

    // Check if there is a desktop camera on a layout and if it is allowed to be there.
    if (!verifyDesktopCameraOnLayout(resourcePool, subject, data))
        return false;

    // Everybody can create own layouts.
    if (subject.user() && data.parentId == subject.user()->getId())
        return true;

    // Only admins can create global layouts.
    if (data.parentId.isNull())
        return hasAdminPermissions(subject);

    // Video wall admins can create layouts on video wall.
    if (const auto videoWall = resourcePool->getResourceById<QnVideoWallResource>(data.parentId))
        return hasAccessRights(subject, videoWall, AccessRight::controlVideowall);

    // Tour owner can create layouts in it.
    const auto tour = m_context->showreelManager()->tour(data.parentId);
    if (tour.isValid())
        return tour.parentId == subject.id();

    const auto ownerResource = resourcePool->getResourceById(data.parentId);

    // We can create layout for user if we can modify this user.
    if (const auto owner = ownerResource.objectCast<QnUserResource>())
        return hasPermission(subject, owner, Qn::SavePermission);

    //< Intercom layout case.
    if (const auto parentCamera = ownerResource.objectCast<QnVirtualCameraResource>())
        return hasAccessRights(subject, parentCamera, AccessRight::userInput);

    return false;
}

bool QnResourceAccessManager::canCreateLayout(
    const QnResourceAccessSubject& subject,
    const QnLayoutResourcePtr& layout) const
{
    nx::vms::api::LayoutData layoutData;
    ec2::fromResourceToApi(layout, layoutData);
    return canCreateLayout(subject, layoutData);
}

bool QnResourceAccessManager::canModifyLayout(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& target,
    const nx::vms::api::LayoutData& update) const
{
    NX_ASSERT(target.dynamicCast<QnLayoutResource>());

    // If we are changing layout parent, it is equal to creating new layout.
    if (target->getParentId() != update.parentId)
        return canCreateLayout(subject, update);

    // Check if there is a desktop camera on a layout and if it is allowed to be there.
    if (!verifyDesktopCameraOnLayout(resourcePool(), subject, update))
        return false;

    // Otherwise - default behavior.
    return hasPermission(subject, target, Qn::SavePermission);
}

//-------------------------------------------------------------------------------------------------
// Storage

bool QnResourceAccessManager::canCreateStorage(const QnResourceAccessSubject& subject,
    const nx::vms::api::StorageData& data) const
{
    return canCreateStorage(subject, data.parentId);
}

bool QnResourceAccessManager::canCreateStorage(const QnResourceAccessSubject& subject,
    const QnUuid& storageParentId) const
{
    if (!subject.isValid())
        return false;

    auto server = resourcePool()->getResourceById<QnMediaServerResource>(storageParentId);
    return hasPermission(subject, server, Qn::SavePermission);
}

bool QnResourceAccessManager::canModifyStorage(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& target,
    const nx::vms::api::StorageData& update) const
{
    NX_ASSERT(target.dynamicCast<QnStorageResource>());

    // Storages cannot be moved from one server to another.
    // Null parentId is allowed because this check is performed before possible API Merge.
    if (!update.parentId.isNull() && target->getParentId() != update.parentId)
        return false;

    /* Otherwise - default behavior. */
    return hasPermission(subject, target, Qn::SavePermission);
}

//-------------------------------------------------------------------------------------------------
// User

bool QnResourceAccessManager::canCreateUser(const QnResourceAccessSubject& subject,
    const nx::vms::api::UserData& data) const
{
    if (data.isCloud && !data.fullName.isEmpty())
        return false; //< Full name for cloud users is allowed to modify via cloud portal only.

    return canCreateUser(subject, data.permissions, data.userRoleIds, data.isAdmin);
}

bool QnResourceAccessManager::canCreateUser(
    const QnResourceAccessSubject& subject,
    GlobalPermissions targetPermissions,
    const std::vector<QnUuid>& targetGroups,
    bool isOwner) const
{
    if (!subject.isValid())
        return false;

    // Nobody can create owners.
    if (isOwner)
        return false;

    if (!userRolesManager()->hasRoles(targetGroups))
        return false;

    const auto effectivePermissions = accumulatePermissions(targetPermissions, targetGroups);

    // Only owner can create admins.
    if (effectivePermissions.testFlag(GlobalPermission::admin))
        return subject.user() && subject.user()->isOwner();

    // Admins can create other users.
    return hasAdminPermissions(subject);
}

bool QnResourceAccessManager::canCreateUser(const QnResourceAccessSubject& subject,
    Qn::UserRole role) const
{
    const auto permissions = QnPredefinedUserRoles::permissions(role);
    const bool isOwner = (role == Qn::UserRole::owner);
    return canCreateUser(subject, permissions, {}, isOwner);
}

bool QnResourceAccessManager::canModifyUser(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& target,
    const nx::vms::api::UserData& update) const
{
    if (!userRolesManager()->hasRoles(update.userRoleIds))
        return false;

    auto userResource = target.dynamicCast<QnUserResource>();
    NX_ASSERT(userResource);

    if (!subject.isValid() || !target)
        return false;

    // Nobody can make user an owner (isAdmin ec2 field) unless target user is already an owner.
    if (!userResource->isOwner() && update.isAdmin)
        return false;

    const auto sourcePermissions = accumulatePermissions(
        userResource->getRawPermissions(), userResource->userRoleIds());

    const auto targetPermissions = accumulatePermissions(update.permissions, update.userRoleIds);

    if (sourcePermissions.testFlag(GlobalPermission::admin)
        || targetPermissions.testFlag(GlobalPermission::admin))
    {
        if (!(subject.user() && subject.user()->isOwner()))
            return false;
    }

    // Changing user type is always prohibited.
    if (userResource->userType() != nx::vms::api::type(update))
        return false;

    /* We should have full access to user to modify him. */
    Qn::Permissions requiredPermissions = Qn::ReadWriteSavePermission;

    if (const auto digest = userResource->getDigest(); digest != update.digest)
    {
        if (userResource->isCloud())
            return false;

        const bool isDigestToggling = (digest == nx::vms::api::UserData::kHttpIsDisabledStub)
            != (update.digest == nx::vms::api::UserData::kHttpIsDisabledStub);

        requiredPermissions |=
            isDigestToggling ? Qn::WriteDigestPermission : Qn::WritePasswordPermission;
    }

    if (target->getName() != update.name)
        requiredPermissions |= Qn::WriteNamePermission;

    if (userResource->getHash().toString() != update.hash)
        requiredPermissions |= Qn::WritePasswordPermission;

    if (sourcePermissions != targetPermissions || userResource->isEnabled() != update.isEnabled)
        requiredPermissions |= Qn::WriteAccessRightsPermission;

    if (userResource->getEmail() != update.email)
        requiredPermissions |= Qn::WriteEmailPermission;

    if (userResource->fullName() != update.fullName)
        requiredPermissions |= Qn::WriteFullNamePermission;

    return hasPermission(subject, target, requiredPermissions);
}

GlobalPermissions QnResourceAccessManager::accumulatePermissions(GlobalPermissions own,
    const std::vector<QnUuid>& parentGroups) const
{
    return std::accumulate(parentGroups.cbegin(), parentGroups.cend(), own,
        [this](GlobalPermissions result, const QnUuid& groupId) -> GlobalPermissions
        {
            return result | m_accessRightsResolver->globalPermissions(groupId);
        });
}

//-------------------------------------------------------------------------------------------------
// Video wall

bool QnResourceAccessManager::canCreateVideoWall(const QnResourceAccessSubject& subject,
    const nx::vms::api::VideowallData& /*data*/) const
{
    return canCreateVideoWall(subject);
}

bool QnResourceAccessManager::canCreateVideoWall(const QnResourceAccessSubject& subject) const
{
    if (!subject.isValid())
        return false;

    /* Only admins can create new videowalls (and attach new screens). */
    return hasAdminPermissions(subject);
}

bool QnResourceAccessManager::canModifyVideoWall(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& target,
    const nx::vms::api::VideowallData& update) const
{
    if (!subject.isValid())
        return false;

    auto videoWallResource = target.dynamicCast<QnVideoWallResource>();
    NX_ASSERT(videoWallResource);

    auto hasItemsChange = [items = videoWallResource->items()->getItems(), update]
    {
        /* Quick check */
        if ((size_t)items.size() != update.items.size())
            return true;

        for (auto updated : update.items)
        {
            if (!items.contains(updated.guid))
                return true;
        }

        return false;
    };

    /* Only admin can add and remove videowall items. */
    if (hasItemsChange())
        return hasAdminPermissions(subject);

    /* Otherwise - default behavior. */
    return hasPermission(subject, target, Qn::SavePermission);
}

//-------------------------------------------------------------------------------------------------
// WebPage

bool QnResourceAccessManager::canCreateWebPage(const QnResourceAccessSubject& subject,
    const nx::vms::api::WebPageData& /*data*/) const
{
    return canCreateWebPage(subject);
}

bool QnResourceAccessManager::canCreateWebPage(const QnResourceAccessSubject& subject) const
{
    if (!subject.isValid())
        return false;

    /* Only admins can add new web pages. */
    return hasAdminPermissions(subject);
}
