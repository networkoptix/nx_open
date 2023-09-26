// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_access_manager.h"

#include <algorithm>

#include <QtCore/QThread>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/global_permissions_watcher.h>
#include <core/resource_access/permissions_cache.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/core/access/access_types.h>
#include <nx/streaming/abstract_archive_resource.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/qt_helpers.h>
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
#include <nx/vms/common/showreel/showreel_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx_ec/data/api_conversion_functions.h>

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

    connect(resourcePool(), &QnResourcePool::resourcesAdded,
        this, &QnResourceAccessManager::handleResourcesAdded, Qt::DirectConnection);

    connect(resourcePool(), &QnResourcePool::resourcesRemoved,
        this, &QnResourceAccessManager::handleResourcesRemoved, Qt::DirectConnection);

    connect(systemContext()->accessSubjectHierarchy(), &SubjectHierarchy::changed, this,
        [this](
            const QSet<QnUuid>& /*added*/,
            const QSet<QnUuid>& /*removed*/,
            const QSet<QnUuid>& /*groupsWithChangedMembers*/,
            const QSet<QnUuid>& subjectsWithChangedParents)
        {
            const auto affectedUsers = resourcePool()->getResourcesByIds<QnUserResource>(
                subjectsWithChangedParents + systemContext()->accessSubjectHierarchy()->
                    recursiveMembers(subjectsWithChangedParents));

            if (!affectedUsers.empty())
                emit permissionsDependencyChanged(affectedUsers);

        }, Qt::DirectConnection);
}

bool QnResourceAccessManager::hasPowerUserPermissions(const Qn::UserAccessData& accessData) const
{
    if (accessData == Qn::kSystemAccess)
        return true;

    return m_accessRightsResolver->hasFullAccessRights(accessData.userId);
}

bool QnResourceAccessManager::hasPowerUserPermissions(const QnResourceAccessSubject& subject) const
{
    return subject.isValid() && (subject.isRole() || subject.user()->isEnabled())
        ? m_accessRightsResolver->hasFullAccessRights(subject.id())
        : false;
}

AccessRights QnResourceAccessManager::accessRights(
    const QnResourceAccessSubject& subject, const QnResourcePtr& resource) const
{
    return subject.isValid() && (subject.isRole() || subject.user()->isEnabled())
        ? m_accessRightsResolver->accessRights(subject.id(), resource)
        : AccessRights{};
}

AccessRights QnResourceAccessManager::accessRights(
    const QnResourceAccessSubject& subject, const QnUuid& resourceGroupId) const
{
    return subject.isValid() && (subject.isRole() || subject.user()->isEnabled())
        ? m_accessRightsResolver->accessRights(subject.id(), resourceGroupId)
        : AccessRights{};
}

bool QnResourceAccessManager::hasAccessRights(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource, AccessRights requiredAccessRights) const
{
    return requiredAccessRights == AccessRights{}
        || (accessRights(subject, resource) & requiredAccessRights) == requiredAccessRights;
}

bool QnResourceAccessManager::hasAccessRights(const QnResourceAccessSubject& subject,
    const QnUuid& resourceGroupId, AccessRights requiredAccessRights) const
{
    return requiredAccessRights == AccessRights{}
        || accessRights(subject, resourceGroupId).testFlags(requiredAccessRights);
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

    return m_accessRightsResolver->globalPermissions(subject.id());
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
    const QnResourcePtr& targetResource) const
{
    if (!subject.isValid() || !targetResource)
        return Qn::NoPermissions;

    // User is already removed.
    if (const auto& user = subject.user())
    {
        if (!user->resourcePool() || user->hasFlags(Qn::removed))
            return Qn::NoPermissions;
    }

    // Resource is already removed.
    if (targetResource->hasFlags(Qn::removed))
        return Qn::NoPermissions;

    // Resource is not added to pool, checking if we can create such resource.
    if (!targetResource->resourcePool())
    {
        const auto result = canCreateResourceInternal(subject, targetResource)
            ? Qn::ReadWriteSavePermission
            : Qn::NoPermissions;
        NX_VERBOSE(this, "Permissions for %1 to create new %2 is %3",
            subject, targetResource, result);
        return result;
    }

    const auto result = calculatePermissions(subject, targetResource);
    NX_VERBOSE(this, "Calculated permissions for %1 over %2 is %3", subject,
        targetResource, result);
    return result;
}

Qn::Permissions QnResourceAccessManager::permissions(const QnResourceAccessSubject& subject,
    const nx::vms::api::UserGroupData& targetGroup) const
{
    if (!subject.isValid() || targetGroup.id.isNull())
        return Qn::NoPermissions;

    // User is already removed.
    if (const auto& user = subject.user())
    {
        if (!user->resourcePool() || user->hasFlags(Qn::removed))
            return Qn::NoPermissions;
    }

    const auto result = calculatePermissionsInternal(subject, targetGroup);
    NX_VERBOSE(this, "Calculated permissions for %1 over %2 is %3", subject, targetGroup, result);
    return result;
}

Qn::Permissions QnResourceAccessManager::permissions(
    const QnResourceAccessSubject& subject, const QnUuid& targetId) const
{
    if (const auto targetResource = resourcePool()->getResourceById(targetId))
        return permissions(subject, targetResource);

    const auto targetGroup = userGroupManager()->find(targetId);
    return targetGroup ? permissions(subject, *targetGroup) : Qn::NoPermissions;
}

bool QnResourceAccessManager::hasPermission(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& targetResource,
    Qn::Permissions requiredPermissions) const
{
    return (permissions(subject, targetResource) & requiredPermissions) == requiredPermissions;
}

bool QnResourceAccessManager::hasPermission(
    const QnResourceAccessSubject& subject,
    const nx::vms::api::UserGroupData& targetGroup,
    Qn::Permissions requiredPermissions) const
{
    return (permissions(subject, targetGroup) & requiredPermissions) == requiredPermissions;
}

bool QnResourceAccessManager::hasPermission(
    const QnResourceAccessSubject& subject,
    const QnUuid& targetId,
    Qn::Permissions requiredPermissions) const
{
    return (permissions(subject, targetId) & requiredPermissions) == requiredPermissions;
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

ResourceAccessDetails QnResourceAccessManager::accessDetails(
    const QnUuid& subjectId, const QnResourcePtr& resource, AccessRight accessRight) const
{
    return m_accessRightsResolver->accessDetails(subjectId, resource, accessRight);
}

QnResourceAccessManager::Notifier* QnResourceAccessManager::createNotifier(QObject* parent)
{
    return m_accessRightsResolver->createNotifier(parent);
}

void QnResourceAccessManager::handleResourcesAdded(const QnResourceList& resources)
{
    for (const auto& resource: resources)
    {
        connect(resource.get(), &QnResource::flagsChanged,
            this, &QnResourceAccessManager::handleResourceUpdated, Qt::DirectConnection);

        if (const auto& camera = resource.objectCast<QnVirtualCameraResource>())
        {
            connect(camera.get(), &QnVirtualCameraResource::scheduleEnabledChanged,
                this, &QnResourceAccessManager::handleResourceUpdated, Qt::DirectConnection);

            connect(camera.get(), &QnVirtualCameraResource::licenseTypeChanged,
                this, &QnResourceAccessManager::handleResourceUpdated, Qt::DirectConnection);

            connect(camera.get(), &QnVirtualCameraResource::capabilitiesChanged,
                this, &QnResourceAccessManager::handleResourceUpdated, Qt::DirectConnection);
        }

        if (const auto& layout = resource.objectCast<QnLayoutResource>())
        {
            connect(layout.get(), &QnLayoutResource::lockedChanged,
                this, &QnResourceAccessManager::handleResourceUpdated, Qt::DirectConnection);
        }

        if (const auto& user = resource.objectCast<QnUserResource>())
        {
            connect(user.get(), &QnUserResource::attributesChanged,
                this, &QnResourceAccessManager::handleResourceUpdated, Qt::DirectConnection);
        }
    }
}

void QnResourceAccessManager::handleResourcesRemoved(const QnResourceList& resources)
{
    NX_MUTEX_LOCKER lk(&m_updatingResourcesMutex);
    for (const auto& resource: resources)
    {
        resource->disconnect(this);
        m_updatingResources.remove(resource);
    }
}

void QnResourceAccessManager::handleResourceUpdated(const QnResourcePtr& resource)
{
    {
        NX_MUTEX_LOCKER lk(&m_updatingResourcesMutex);
        if (isUpdating())
        {
            m_updatingResources.insert(resource);
            return;
        }
    }

    NX_VERBOSE(this, "Permissions dependency changed for %1", resource);
    emit permissionsDependencyChanged({resource});
}

void QnResourceAccessManager::afterUpdate()
{
    const QnResourceList updatedResources =
        [this]()
        {
            NX_MUTEX_LOCKER lk(&m_updatingResourcesMutex);
            const auto result = m_updatingResources.values();
            m_updatingResources = {};
            return result;
        }();

    if (updatedResources.empty())
        return;

    NX_VERBOSE(this, "Permissions dependencies changed for %1", updatedResources);
    emit permissionsDependencyChanged(updatedResources);
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
            targetUser->groupIds());
    }

    if (const auto storage = target.dynamicCast<QnStorageResource>())
        return canCreateStorage(subject, storage->getParentId());

    if (const auto videoWall = target.dynamicCast<QnVideoWallResource>())
        return canCreateVideoWall(subject);

    if (const auto webPage = target.dynamicCast<QnWebPageResource>())
        return canCreateWebPage(subject);

    return hasPowerUserPermissions(subject);
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
    if (hasPowerUserPermissions(subject))
        result |= Qn::RemovePermission;

    const auto accessRights = this->accessRights(subject, camera);
    if (!accessRights)
        return result;

    result |= Qn::ReadPermission;

    constexpr AccessRights kViewContentRequirements = AccessRight::view
        | AccessRight::viewArchive;

    if (accessRights.testAnyFlags(kViewContentRequirements))
        result |= Qn::ViewContentPermission;

    const bool isLiveAllowed = !camera->needsToChangeDefaultPassword()
        && accessRights.testFlag(AccessRight::view);

    bool isFootageAllowed = accessRights.testFlag(AccessRight::viewArchive);
    bool isExportAllowed = accessRights.testFlag(AccessRight::exportArchive);

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
        result |= Qn::ExportPermission;

    if (accessRights.testFlag(AccessRight::viewBookmarks))
        result |= Qn::ViewBookmarksPermission;

    if (accessRights.testFlag(AccessRight::manageBookmarks))
        result |= Qn::ManageBookmarksPermission;

    if (accessRights.testFlag(AccessRight::userInput))
        result |= Qn::UserInputPermissions;

    if (accessRights.testFlag(AccessRight::edit))
        result |= Qn::GenericEditPermissions;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnMediaServerResourcePtr& server) const
{
    if (hasPowerUserPermissions(subject))
    {
        Qn::Permissions result = Qn::FullServerPermissions;
        if (!hasGlobalPermission(subject, GlobalPermission::administrator))
            result &= ~Qn::RemovePermission;
        return result;
    }

    Qn::Permissions result = Qn::ReadPermission;

    if (hasAccessRights(subject, server, AccessRight::view))
        result |= Qn::ViewContentPermission;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnStorageResourcePtr& storage) const
{
    const auto parentServer = storage->getParentServer();

    //< Cloud storage isn't tied to any server and is readable for all.
    if (!parentServer)
    {
        if (hasPowerUserPermissions(subject))
            return Qn::ReadWriteSavePermission;

        return Qn::ReadPermission;
    }

    if (hasPowerUserPermissions(subject))
        return Qn::ReadWriteSavePermission | Qn::RemovePermission;

    if (permissions(subject, parentServer).testFlag(Qn::SavePermission))
        return Qn::ReadWriteSavePermission;

    return Qn::NoPermissions;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnVideoWallResourcePtr& videoWall) const
{
    if (!hasAccessRights(subject, videoWall, AccessRight::edit))
        return Qn::NoPermissions;

    Qn::Permissions result =
          Qn::ReadPermission
        | Qn::WritePermission
        | Qn::SavePermission
        | Qn::WriteNamePermission;

    if (hasPowerUserPermissions(subject))
        result |= Qn::RemovePermission;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnWebPageResourcePtr& webPage) const
{
    if (hasPowerUserPermissions(subject))
        return Qn::FullWebPagePermissions;

    const auto accessRights = this->accessRights(subject, webPage);
    if (!accessRights)
        return Qn::NoPermissions;

    Qn::Permissions result = Qn::ReadPermission;

    if (accessRights.testFlag(AccessRight::view))
        result |= Qn::ViewContentPermission;

    if (accessRights.testFlag(AccessRight::edit))
        result |= Qn::GenericEditPermissions | Qn::RemovePermission;

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
                | Qn::WriteNamePermission;

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
            // Administrator can do anything.
            if (subject.user() && subject.user()->isAdministrator())
                return Qn::FullLayoutPermissions;

            const auto ownerId = layout->getParentId();

            // Access to global layouts.
            // Simple check is enough, exported layouts are checked on the client side.
            const bool isShared = layout->isShared();
            const bool isIntercom = !isShared &&
                [&]()
                {
                    const auto camera = resourcePool()->getResourceById<QnResource>(ownerId);
                    return nx::vms::common::isIntercom(camera);
                }();

            if (isShared || isIntercom)
            {
                if (!accessRights)
                    return Qn::NoPermissions;

                // Global layouts editor.
                if (hasPowerUserPermissions(subject))
                    return Qn::FullLayoutPermissions;

                return Qn::ModifyLayoutPermission;
            }

            // Access to videowall layouts.
            const auto videowall = resourcePool()->getResourceById<QnVideoWallResource>(ownerId);
            if (videowall)
            {
                return hasAccessRights(subject, videowall, AccessRight::edit)
                    ? Qn::FullLayoutPermissions
                    : Qn::NoPermissions;
            }

            // Checking other user's layout. Only users can have access to them.
            if (const auto user = subject.user())
            {
                if (ownerId != user->getId())
                {
                    const auto owner = resourcePool()->getResourceById<QnUserResource>(ownerId);
                    if (!owner)
                    {
                        const auto showreel = m_context->showreelManager()->showreel(ownerId);
                        if (showreel.isValid())
                        {
                            return showreel.parentId == user->getId()
                                ? Qn::FullLayoutPermissions
                                : Qn::NoPermissions;
                        }

                        // Layout of user, which we don't know of.
                        return hasPowerUserPermissions(subject)
                            ? Qn::FullLayoutPermissions
                            : Qn::NoPermissions;
                    }

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
    const bool isSubjectAdministrator = subject.user() && subject.user()->isAdministrator();

    const auto filterByUserType =
        [targetUser](Qn::Permissions permissions)
        {
            if (targetUser->attributes().testFlag(UserAttribute::readonly))
                return permissions & Qn::ReadPermission;

            switch (targetUser->userType())
            {
                case nx::vms::api::UserType::ldap:
                {
                    return permissions & ~(
                        Qn::WriteNamePermission
                        | Qn::WriteFullNamePermission
                        | Qn::WritePasswordPermission
                        | Qn::WriteEmailPermission);
                }
                case nx::vms::api::UserType::cloud:
                {
                    return permissions & ~(
                        Qn::WritePasswordPermission
                        | Qn::WriteDigestPermission
                        | Qn::WriteNamePermission
                        | Qn::WriteEmailPermission
                        | Qn::WriteFullNamePermission);
                }
                default:
                {
                    return permissions;
                }
            }
        };

    // Permissions on self.

    if (targetUser == subject.user())
    {
        // Everyone can edit own data, except own access rights.
        Qn::Permissions result = Qn::ReadWriteSavePermission | Qn::WritePasswordPermission
            | Qn::WriteEmailPermission | Qn::WriteFullNamePermission;

        if (isSubjectAdministrator)
            result |= Qn::WriteDigestPermission;

        return filterByUserType(result);
    }

    // Permissions on others.

    if (!hasPowerUserPermissions(subject))
        return Qn::NoPermissions;

    if (targetUser->isAdministrator())
    {
        if (isSubjectAdministrator && !targetUser->isCloud())
            return filterByUserType(Qn::ReadWriteSavePermission | Qn::WriteDigestPermission);

        return Qn::ReadPermission;
    }

    if (isSubjectAdministrator || !m_accessRightsResolver->hasFullAccessRights(targetUser->getId()))
        return filterByUserType(Qn::FullUserPermissions);

    return Qn::ReadPermission;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const nx::vms::api::UserGroupData& targetGroup) const
{
    if (targetGroup.id.isNull())
        return Qn::NoPermissions;

    Qn::Permissions result = Qn::NoPermissions;
    const bool isSubjectAdministrator = subject.user() && subject.user()->isAdministrator();

    if (hasPowerUserPermissions(subject))
    {
        result = Qn::ReadPermission;

        if (!targetGroup.attributes.testFlag(nx::vms::api::UserAttribute::readonly))
        {
            // Power Users can only be edited by Administrators, other groups - by all Power Users.
            if (isSubjectAdministrator || !m_accessRightsResolver->hasFullAccessRights(targetGroup.id))
                result |= Qn::FullGenericPermissions | Qn::WriteAccessRightsPermission;

            if (targetGroup.type == UserType::ldap)
                result.setFlag(Qn::WriteNamePermission, false);
        }
    }

    if (targetGroup.attributes.testFlag(UserAttribute::readonly))
        return result & Qn::ReadPermission;

    return result;
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

    // Only power users can create global layouts.
    if (data.parentId.isNull())
        return hasPowerUserPermissions(subject);

    // Users with access to video walls can create layouts on them.
    if (const auto videoWall = resourcePool->getResourceById<QnVideoWallResource>(data.parentId))
        return hasAccessRights(subject, videoWall, AccessRight::edit);

    // Showreel owner can create layouts in it.
    const auto showreel = m_context->showreelManager()->showreel(data.parentId);
    if (showreel.isValid())
        return showreel.parentId == subject.id();

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
    const auto layout = target.dynamicCast<QnLayoutResource>();
    if (!NX_ASSERT(layout))
        return false;

    // If we are changing layout parent, it is equal to creating new layout.
    if (target->getParentId() != update.parentId)
        return canCreateLayout(subject, update);

    // Check if there is a desktop camera on a layout and if it is allowed to be there.
    if (!verifyDesktopCameraOnLayout(resourcePool(), subject, update))
        return false;

    // Check if the user has permission to save this layout.
    if (!hasPermission(subject, target, Qn::SavePermission))
        return false;

    // If the layout is not owned by the subject, check there are no items being placed to which
    // the subject would gain new access rights.
    if (layout->getParentId() != subject.id() && !hasPowerUserPermissions(subject))
    {
        const auto layoutAccessRights = accessRights(subject, layout);

        QSet<QnUuid> oldResourceIds;
        for (const auto& item: layout->getItems())
            oldResourceIds.insert(item.resource.id);

        for (const auto& item: update.items)
        {
            if (oldResourceIds.contains(item.resourceId))
                continue;

            // We should not allow user to place some camera, which does not exist in the system
            // right now, but can potentially be added in the future. From the other side, we
            // should allow user to save layouts with local files. Also this can lead to issues
            // only when camera is added to the shared layout, but only power users can do it, so
            // it can be considered safe. Allow adding not-existent resources for now.
            const auto resource = resourcePool()->getResourceById(item.resourceId);
            if (!resource)
                continue;

            if (!hasAccessRights(subject, resource, layoutAccessRights))
                return false;
        }
    }

    return true;
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
    if (data.type == UserType::cloud && !data.fullName.isEmpty())
        return false; //< Full name for cloud users is allowed to modify via cloud portal only.

    return canCreateUser(subject, data.permissions, data.groupIds);
}

bool QnResourceAccessManager::canCreateUser(
    const QnResourceAccessSubject& subject,
    GlobalPermissions newPermissions,
    const std::vector<QnUuid>& targetGroups) const
{
    if (!subject.isValid())
        return false;

    if ((newPermissions & ~kAssignableGlobalPermissions) != 0)
        return false;

    if (!nx::vms::common::allUserGroupsExist(systemContext(), targetGroups))
        return false;

    const auto effectivePermissions = accumulatePermissions(newPermissions, targetGroups);

    // No one can create Administrators.
    if (effectivePermissions.testFlag(GlobalPermission::administrator))
        return false;

    // Only Administrator can create Power Users.
    if (effectivePermissions.testFlag(GlobalPermission::powerUser))
        return subject.user() && subject.user()->isAdministrator();

    // Power Users can create other users.
    return hasPowerUserPermissions(subject);
}

bool QnResourceAccessManager::canModifyUser(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& target,
    const nx::vms::api::UserData& update) const
{
    if (!nx::vms::common::allUserGroupsExist(systemContext(), update.groupIds))
        return false;

    if ((update.permissions & ~kAssignableGlobalPermissions) != 0)
        return false;

    auto userResource = target.dynamicCast<QnUserResource>();
    if (!subject.isValid() || !NX_ASSERT(userResource))
        return false;

    const auto oldPermissions = accumulatePermissions(
        userResource->getRawPermissions(), userResource->groupIds());

    const auto newPermissions = accumulatePermissions(update.permissions, update.groupIds);

    const auto accessRightMap =
        systemContext()->accessRightsManager()->ownResourceAccessMap(update.id);

    const bool permissionsChanged = oldPermissions != newPermissions
        || userResource->getRawPermissions() != update.permissions
        || nx::utils::toQSet(userResource->groupIds()) != nx::utils::toQSet(update.groupIds)
        || userResource->isEnabled() != update.isEnabled
        || accessRightMap != QHash<QnUuid, AccessRights>{
            update.resourceAccessRights.begin(), update.resourceAccessRights.end()};

    // Cannot modify own permissions.
    if (subject.user() == target && permissionsChanged)
        return false;

    // Power User permissions can be granted or revoked only by an Administrator.
    if (oldPermissions.testFlag(GlobalPermission::powerUser)
        != newPermissions.testFlag(GlobalPermission::powerUser))
    {
        if (!(subject.user() && subject.user()->isAdministrator()))
            return false;
    }

    // Administrator permissions cannot be granted or revoked.
    if (oldPermissions.testFlag(GlobalPermission::administrator)
        != newPermissions.testFlag(GlobalPermission::administrator))
    {
        return false;
    }

    // Changing user type is always prohibited.
    if (userResource->userType() != update.type)
        return false;

    // We should have full access to user to modify him.
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

    if (!userResource->getHash().compareToPasswordHash(update.hash))
        requiredPermissions |= Qn::WritePasswordPermission;

    if (permissionsChanged)
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

    // Only power users can create new Videowalls (and attach new screens).
    return hasPowerUserPermissions(subject);
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

    // Only power users can add and remove Videowall items.
    if (hasItemsChange())
        return hasPowerUserPermissions(subject);

    // Otherwise - default behavior.
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

    /* Only power users can add new web pages. */
    return hasPowerUserPermissions(subject);
}

bool QnResourceAccessManager::hasAccessToAllCameras(
    const QnUuid& userId,
    nx::vms::api::AccessRights accessRights) const
{
    const auto user = resourcePool()->getResourceById<QnUserResource>(userId);
    if (!user)
        return false;

    if (user->isAdministrator())
        return true;

    return hasAccessRights(user, nx::vms::api::kAllDevicesGroupId, accessRights);
}

bool QnResourceAccessManager::hasAccessToAllCameras(
    const Qn::UserAccessData& userAccessData,
    nx::vms::api::AccessRights accessRights) const
{
    return hasAccessToAllCameras(userAccessData.userId, accessRights);
}
