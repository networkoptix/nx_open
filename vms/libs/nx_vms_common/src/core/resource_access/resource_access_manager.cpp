// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_access_manager.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/permissions_cache.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/resource_access_subjects_cache.h>
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

bool isPermissionsUpdateAttempted(const QnUserResourcePtr& existingUser,
    GlobalPermissions permissions)
{
    if (permissions.testFlag(GlobalPermission::none))
        return false;

    return existingUser->getRawPermissions() != permissions;
}

bool canUpdatePermissions(
    const QnUserResourcePtr& requestSender, const nx::vms::api::UserData& param)
{
    if (requestSender->isOwner())
        return !param.isAdmin;

    if (requestSender->getRawPermissions().testFlag(GlobalPermission::admin))
        return !param.permissions.testFlag(GlobalPermission::admin);

    return false;
}

} // namespace

QnResourceAccessManager::QnResourceAccessManager(
    Mode mode,
    nx::vms::common::SystemContext* context,
    QObject* parent)
    :
    base_type(parent),
    nx::vms::common::SystemContextAware(context),
    m_mode(mode),
    m_permissionsCache(std::make_unique<PermissionsCache>())
{
    if (m_mode == Mode::cached)
    {
        const auto& resPool = resourcePool();

        connect(m_context->resourceAccessProvider(),
            &ResourceAccessProvider::accessChanged,
            this,
            &QnResourceAccessManager::updatePermissions);

        connect(m_context->globalPermissionsManager(),
            &QnGlobalPermissionsManager::globalPermissionsChanged,
            this,
            &QnResourceAccessManager::updatePermissionsBySubject);

        connect(resPool, &QnResourcePool::resourceAdded, this,
            &QnResourceAccessManager::handleResourceAdded);
        connect(resPool, &QnResourcePool::resourceRemoved, this,
            &QnResourceAccessManager::handleResourceRemoved);

        connect(m_context->userRolesManager(), &QnUserRolesManager::userRoleRemoved, this,
            &QnResourceAccessManager::handleUserRoleRemoved);

        recalculateAllPermissions();
    }
}

void QnResourceAccessManager::setPermissionsInternal(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource, Qn::Permissions permissions)
{
    if (!subject.isValid())
        return;

    auto isValid = [&]
        {
            if (!resource->resourcePool())
                return false;

            if (const auto user = subject.user())
            {
                // TODO: Find out if we want to check: user->isEnabled()
                // TODO: fix the problem with dangling role ids and return check for:
                //     userRolesManager()->hasRoles(user->userRoleIds())
                return user->resourcePool() != nullptr;
            }

            return m_context->userRolesManager()->hasRole(subject.id());
        };

    if (isValid())
    {
        // Store permissions in cache.
        std::unique_lock lock(m_permissionsCacheMutex);
        if (!m_permissionsCache->setPermissions(subject.id(), resource->getId(), permissions))
            return;
    }
    else
    {
        // Just check if need to send the signal.
        std::shared_lock lock(m_permissionsCacheMutex);
        if (auto storedPermissions =
            m_permissionsCache->permissions(subject.id(), resource->getId()))
        {
            if (*storedPermissions == permissions)
                return;
        }
    }

    NX_DEBUG(this, "%1 -> %2: %3", subject, resource->getName(), permissions);
    emit permissionsChanged(subject, resource, permissions);
}

GlobalPermissions QnResourceAccessManager::globalPermissions(
    const QnResourceAccessSubject& subject) const
{
    return m_context->globalPermissionsManager()->globalPermissions(subject);
}

bool QnResourceAccessManager::hasGlobalPermission(
    const Qn::UserAccessData& accessRights,
    GlobalPermission requiredPermission) const
{
    return m_context->globalPermissionsManager()->hasGlobalPermission(
        accessRights, requiredPermission);
}

bool QnResourceAccessManager::hasGlobalPermission(
    const QnResourceAccessSubject& subject,
    GlobalPermission requiredPermission) const
{
    return m_context->globalPermissionsManager()->hasGlobalPermission(subject, requiredPermission);
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

    if (m_mode == Mode::cached)
    {
        const auto subjectId = subject.id();
        const auto resourceId = resource->getId();
        {
            std::shared_lock lock(m_permissionsCacheMutex);
            if (auto permissions = m_permissionsCache->permissions(subjectId, resourceId))
            {
                NX_TRACE(this, "Cached permissions for %1 ower %2 is %3",
                    subject, resource, *permissions);
                return *permissions;
            }
        }
        const auto result = calculatePermissions(subject, resource);
        NX_TRACE(this, "Caching calculated permissions for %1 ower %2 is %3",
            subject, resourceId, result);
        {
            std::unique_lock lock(m_permissionsCacheMutex);
            m_permissionsCache->setPermissions(subjectId, resourceId, result);
        }
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

bool QnResourceAccessManager::canCreateResourceInternal(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& target) const
{
    NX_ASSERT(subject.isValid());
    NX_ASSERT(target);
    NX_ASSERT(!isUpdating());

    if (!subject.isValid())
        return false;

    /* Check new layouts creating. */
    if (QnLayoutResourcePtr layout = target.dynamicCast<QnLayoutResource>())
        return canCreateLayout(subject, layout);

    /* Check new users creating. */
    if (QnUserResourcePtr targetUser = target.dynamicCast<QnUserResource>())
        return canCreateUser(subject, targetUser->getRawPermissions(), targetUser->isOwner());

    if (QnStorageResourcePtr storage = target.dynamicCast<QnStorageResource>())
        return canCreateStorage(subject, storage->getParentId());

    if (QnVideoWallResourcePtr videoWall = target.dynamicCast<QnVideoWallResource>())
        return canCreateVideoWall(subject);

    if (QnWebPageResourcePtr webPage = target.dynamicCast<QnWebPageResource>())
        return canCreateWebPage(subject);

    return hasGlobalPermission(subject, GlobalPermission::admin);
}

void QnResourceAccessManager::beforeUpdate()
{
    if (m_mode == Mode::direct)
        return;

    // Cache cleared here, otherwise resources which were removed during update may dangle.
    std::unique_lock lock(m_permissionsCacheMutex);
    m_permissionsCache->clear();
}

void QnResourceAccessManager::afterUpdate()
{
    if (m_mode == Mode::direct)
        return;

    recalculateAllPermissions();
}

void QnResourceAccessManager::recalculateAllPermissions()
{
    NX_ASSERT(m_mode == Mode::cached);
    QN_LOG_TIME(Q_FUNC_INFO);

    if (isUpdating())
        return;

    auto resources = resourcePool()->getResources();
    auto subjects = m_context->resourceAccessSubjectsCache()->allSubjects();

    nx::utils::erase_if(resources,
        [this](const auto& resource)
        {
            return resource->resourcePool() == nullptr;
        });

    std::erase_if(subjects,
        [this](const auto& subject)
        {
            if (const auto user = subject.user())
            {
                return !user->isEnabled() || !user->resourcePool()
                    || !m_context->userRolesManager()->hasRoles(user->userRoleIds());
            }
            return !m_context->userRolesManager()->hasRole(subject.id());
        });

    auto permissionsCache = std::make_unique<PermissionsCache>();
    for (const auto& subject: std::as_const(subjects))
    {
        auto globalPermissions = m_context->globalPermissionsManager()->globalPermissions(subject);
        auto accessibleResources =
            m_context->resourceAccessProvider()->accessibleResources(subject);
        for (const QnResourcePtr& resource: std::as_const(resources))
        {
            bool hasAccessToResoure = accessibleResources.contains(resource->getId());
            permissionsCache->setPermissions(subject.id(), resource->getId(),
                calculatePermissions(subject, resource, &globalPermissions, &hasAccessToResoure));
        }
    }
    {
        std::unique_lock lock(m_permissionsCacheMutex);
        m_permissionsCache = std::move(permissionsCache);
    }
    emit allPermissionsRecalculated();
}

void QnResourceAccessManager::updatePermissions(const QnResourceAccessSubject& subject,
    const QnResourcePtr& target)
{
    NX_ASSERT(m_mode == Mode::cached);

    if (isUpdating())
        return;

    setPermissionsInternal(subject, target, calculatePermissions(subject, target));
}

void QnResourceAccessManager::updatePermissionsToResource(const QnResourcePtr& resource)
{
    if (isUpdating())
        return;

    for (const auto& subject: m_context->resourceAccessSubjectsCache()->allSubjects())
        updatePermissions(subject, resource);
}

void QnResourceAccessManager::updatePermissionsBySubject(const QnResourceAccessSubject& subject)
{
    if (isUpdating())
        return;

    for (const QnResourcePtr& resource: resourcePool()->getResources())
        updatePermissions(subject, resource);
}

void QnResourceAccessManager::handleResourceAdded(const QnResourcePtr& resource)
{
    if (const auto& layout = resource.dynamicCast<QnLayoutResource>())
    {
        /* If layout become shared AND user is admin - he will not receive access notification
         * (because he already had access) but permissions must be recalculated. */
        connect(layout.get(), &QnResource::parentIdChanged, this,
             &QnResourceAccessManager::updatePermissionsToResource);
        connect(layout.get(), &QnLayoutResource::lockedChanged, this,
            &QnResourceAccessManager::updatePermissionsToResource);
    }

    if (const auto& camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        connect(camera.get(), &QnVirtualCameraResource::licenseTypeChanged, this,
            &QnResourceAccessManager::updatePermissionsToResource);

        connect(camera.get(), &QnVirtualCameraResource::scheduleEnabledChanged, this,
            &QnResourceAccessManager::updatePermissionsToResource);

        connect(camera.get(), &QnVirtualCameraResource::capabilitiesChanged, this,
            &QnResourceAccessManager::updatePermissionsToResource);
    }

    if (isUpdating())
        return;

    updatePermissionsToResource(resource);
    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
        updatePermissionsBySubject(user);
}

void QnResourceAccessManager::handleResourceRemoved(const QnResourcePtr& resource)
{
    resource->disconnect(this);

    if (isUpdating())
        return;

    const auto resourceId = resource->getId();

    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
    {
        PermissionsCache::ResourceIdsWithPermissions resourcesWithPermissions;
        {
            std::shared_lock lock(m_permissionsCacheMutex);
            resourcesWithPermissions = m_permissionsCache->permissionsForSubject(user->getId());
        }

        QnUuidList resourceIdsToNotify;
        for (const auto& resourceWithPermission: resourcesWithPermissions)
        {
            if (!resourceWithPermission.second || *resourceWithPermission.second != Qn::NoPermissions)
                resourceIdsToNotify.append(resourceWithPermission.first);
        }
        for (const auto& resource: resourcePool()->getResourcesByIds(resourceIdsToNotify))
            emit permissionsChanged(user, resource, Qn::NoPermissions);
    }

    {
        std::unique_lock lock(m_permissionsCacheMutex);
        m_permissionsCache->removeResource(resourceId);
    }

    for (const auto& subject: m_context->resourceAccessSubjectsCache()->allSubjects())
        emit permissionsChanged(subject, resource, Qn::NoPermissions);
}


void QnResourceAccessManager::handleUserRoleRemoved(const QnResourceAccessSubject& subject)
{
    if (isUpdating())
        return;

    NX_ASSERT(subject.isRole());
    const auto subjectId = subject.id();

    PermissionsCache::ResourceIdsWithPermissions resourcesWithPermissions;
    {
        std::unique_lock lock(m_permissionsCacheMutex);
        resourcesWithPermissions = m_permissionsCache->permissionsForSubject(subjectId);
        m_permissionsCache->removeSubject(subjectId);
    }

    QnUuidList resourceIdsToNotify;
    for (const auto& resourceWithPermission: resourcesWithPermissions)
    {
        if (!resourceWithPermission.second || *resourceWithPermission.second != Qn::NoPermissions)
            resourceIdsToNotify.append(resourceWithPermission.first);
    }

    for (const auto& resource: resourcePool()->getResourcesByIds(resourceIdsToNotify))
        emit permissionsChanged(subject, resource, Qn::NoPermissions);
}

Qn::Permissions QnResourceAccessManager::calculatePermissions(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& target,
    GlobalPermissions* globalPermissionsHint,
    bool* hasAccessToResourceHint) const
{
    NX_ASSERT(target);
    if (subject.user() && !subject.user()->isEnabled())
        return Qn::NoPermissions;

    if (!target || !target->resourcePool())
        return Qn::NoPermissions;

    auto globalPermissions = globalPermissionsHint
        ? *globalPermissionsHint
        : m_context->globalPermissionsManager()->globalPermissions(subject);

    bool hasAccessToResource = hasAccessToResourceHint
        ? *hasAccessToResourceHint
        : m_context->resourceAccessProvider()->hasAccess(subject, target);

    const auto flags = target->flags();

    if (flags.testFlag(Qn::user))
    {
        const auto targetUser = target.dynamicCast<QnUserResource>();
        if (NX_ASSERT(targetUser))
        {
            return calculatePermissionsInternal(
                subject, targetUser, globalPermissions, hasAccessToResource);
        }
    }
    else if (flags.testFlag(Qn::layout))
    {
        const auto layout = target.dynamicCast<QnLayoutResource>();
        if (NX_ASSERT(layout))
        {
            return calculatePermissionsInternal(
                subject, layout, globalPermissions, hasAccessToResource);
        }
    }
    else if (flags.testFlag(Qn::server))
    {
        const auto server = target.dynamicCast<QnMediaServerResource>();
        if (NX_ASSERT(server))
        {
            return calculatePermissionsInternal(
                subject, server, globalPermissions, hasAccessToResource);
        }
    }
    else if (flags.testFlag(Qn::videowall))
    {
        const auto videowall = target.dynamicCast<QnVideoWallResource>();
        if (NX_ASSERT(videowall))
        {
            return calculatePermissionsInternal(
                subject, videowall, globalPermissions, hasAccessToResource);
        }
    }
    else if (flags.testFlag(Qn::web_page))
    {
        const auto webPage = target.dynamicCast<QnWebPageResource>();
        if (NX_ASSERT(webPage))
        {
            return calculatePermissionsInternal(
                subject, webPage, globalPermissions, hasAccessToResource);
        }
    }

    if (const auto camera = target.dynamicCast<QnVirtualCameraResource>())
    {
        return calculatePermissionsInternal(
            subject, camera, globalPermissions, hasAccessToResource);
    }

    if (const auto storage = target.dynamicCast<QnStorageResource>())
    {
        return calculatePermissionsInternal(
            subject, storage, globalPermissions, hasAccessToResource);
    }

    if (QnAbstractArchiveResourcePtr archive = target.dynamicCast<QnAbstractArchiveResource>())
        return Qn::ReadPermission | Qn::ExportPermission;

    if (const auto plugin = target.dynamicCast<AnalyticsPluginResource>())
        return Qn::ReadPermission;

    if (const auto engine = target.dynamicCast<AnalyticsEngineResource>())
        return Qn::ReadPermission;

    NX_ASSERT(false, "invalid resource type");
    return Qn::NoPermissions;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& /*subject*/,
    const QnVirtualCameraResourcePtr& camera,
    GlobalPermissions globalPermissions,
    bool hasAccessToResource) const
{
    Qn::Permissions result = Qn::NoPermissions;

    /* Admins must be able to remove any cameras to delete servers.  */
    if (globalPermissions.testFlag(GlobalPermission::admin))
        result |= Qn::RemovePermission;

    if (!hasAccessToResource)
        return result;

    result |= Qn::ReadPermission | Qn::ViewContentPermission;

    bool isLiveAllowed = !camera->needsToChangeDefaultPassword();
    bool isFootageAllowed = globalPermissions.testFlag(GlobalPermission::viewArchive);
    bool isExportAllowed = isFootageAllowed
        && globalPermissions.testFlag(GlobalPermission::exportArchive);

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

    if (globalPermissions.testFlag(GlobalPermission::userInput))
        result |= Qn::WritePtzPermission;

    if (globalPermissions.testFlag(GlobalPermission::editCameras))
        result |= Qn::ReadWriteSavePermission | Qn::WriteNamePermission;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& /*subject*/,
    const QnMediaServerResourcePtr& /*server*/,
    GlobalPermissions globalPermissions,
    bool hasAccessToResource) const
{
    Qn::Permissions result = Qn::ReadPermission;
    if (!hasAccessToResource)
        return result;

    result |= Qn::ViewContentPermission;

    if (globalPermissions.testFlag(GlobalPermission::admin))
        result |= Qn::FullServerPermissions;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject,
    const QnStorageResourcePtr& storage,
    GlobalPermissions /*globalPermissions*/,
    bool /*hasAccessToResource*/) const
{
    const auto parentServer = storage->getParentServer();
    if (!parentServer)
        return Qn::ReadPermission; //< Cloud storage is not tied to any server and is readable for all.

    const auto serverPermissions = permissions(subject, parentServer);

    if (serverPermissions.testFlag(Qn::RemovePermission))
        return Qn::ReadWriteSavePermission | Qn::RemovePermission;

    if (serverPermissions.testFlag(Qn::SavePermission))
        return Qn::ReadWriteSavePermission;

    return Qn::NoPermissions;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject,
    const QnVideoWallResourcePtr& /*videoWall*/,
    GlobalPermissions globalPermissions,
    bool /*hasAccessToResource*/) const
{
    Qn::Permissions result = Qn::NoPermissions;
    if (!hasGlobalPermission(subject, GlobalPermission::controlVideowall))
        return result;

    result = Qn::ReadPermission | Qn::WritePermission;
    result |= Qn::SavePermission | Qn::WriteNamePermission;
    if (globalPermissions.testFlag(GlobalPermission::admin))
        result |= Qn::RemovePermission;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& /*subject*/,
    const QnWebPageResourcePtr& /*webPage*/,
    GlobalPermissions globalPermissions,
    bool hasAccessToResource) const
{
    if (!hasAccessToResource)
        return Qn::NoPermissions;

    Qn::Permissions result = Qn::ReadPermission | Qn::ViewContentPermission;

    /* Web Page behaves totally like camera. */
    if (globalPermissions.testFlag(GlobalPermission::editCameras))
        result |= Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject,
    const QnLayoutResourcePtr& layout,
    GlobalPermissions globalPermissions,
    bool hasAccessToResource) const
{
    if (!subject.isValid())
        return Qn::NoPermissions;

    // TODO: #sivanov Code duplication with the same QnWorkbenchAccessController method.

    auto checkLocked =
        [this, layout](const Qn::Permissions permissions)
        {
            if (!layout->locked())
                return permissions;

            Qn::Permissions forbidden = Qn::AddRemoveItemsPermission | Qn::WriteNamePermission | Qn::WritePermission;

            // Autogenerated layouts (e.g. videowall) can be removed even when locked
            if (!layout->isServiceLayout())
                forbidden |= Qn::RemovePermission;

            return permissions & ~forbidden;
        };

    /* Layouts with desktop cameras are not to be modified but can be removed. */
    const auto items = layout->getItems();
    for (auto item: items)
    {
        if (const auto& resource = resourcePool()->getResourceByDescriptor(item.resource))
        {
            if (resource->hasFlags(Qn::desktop_camera))
                return Qn::ReadPermission | Qn::RemovePermission;
        }
    }

    /* Calculate base layout permissions */
    auto base = [&]() -> Qn::Permissions
        {
            /* Owner can do anything. */
            if (subject.user() && subject.user()->isOwner())
                return Qn::FullLayoutPermissions;

            /* Access to global layouts. Simple check is enough, exported layouts are checked on the client side. */
            if (layout->isShared())
            {
                if (!hasAccessToResource)
                    return Qn::NoPermissions;

                /* Global layouts editor. */
                if (globalPermissions.testFlag(GlobalPermission::admin))
                    return Qn::FullLayoutPermissions;

                return Qn::ModifyLayoutPermission;
            }

            QnUuid ownerId = layout->getParentId();

            /* Access to videowall layouts. */
            const auto videowall = resourcePool()->getResourceById<QnVideoWallResource>(ownerId);
            if (videowall)
            {
                /* Videowall layout. */
                return globalPermissions.testFlag(GlobalPermission::controlVideowall)
                    ? Qn::FullLayoutPermissions
                    : Qn::NoPermissions;
            }

            /* Access to intercom layouts. */
            const auto camera = resourcePool()->getResourceById<QnResource>(ownerId);
            if (nx::vms::common::isIntercom(camera))
            {
                if (globalPermissions.testFlag(GlobalPermission::userInput))
                    return Qn::FullLayoutPermissions;

                return Qn::ReadPermission;
            }

            /* Checking other user's layout. Only users can have access to them. */
            if (auto user = subject.user())
            {
                if (ownerId != user->getId())
                {
                    const auto owner = resourcePool()->getResourceById<QnUserResource>(ownerId);
                    if (!owner)
                    {
                        const auto tour = m_context->layoutTourManager()->tour(ownerId);
                        if (tour.isValid())
                        {
                            return tour.parentId == user->getId()
                                ? Qn::FullLayoutPermissions
                                : Qn::NoPermissions;
                        }

                        /* Layout of user, which we don't know of. */
                        return globalPermissions.testFlag(GlobalPermission::admin)
                            ? Qn::FullLayoutPermissions
                            : Qn::NoPermissions;
                    }

                    /* We can modify layout for user if we can modify this user. */
                    Qn::Permissions userPermissions = permissions(subject, owner);
                    if (userPermissions.testFlag(Qn::SavePermission))
                        return Qn::FullLayoutPermissions;

                    /* We can see layouts for another users if we are able to see these users. */
                    if (userPermissions.testFlag(Qn::ReadPermission))
                        return Qn::ModifyLayoutPermission;

                    return Qn::NoPermissions;
                }
            }

            /* User can do whatever he wants with own layouts. */
            return Qn::FullLayoutPermissions;
        };

    return checkLocked(base());
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject,
    const QnUserResourcePtr& targetUser,
    GlobalPermissions globalPermissions,
    bool /*hasAccessToResource*/) const
{
    auto checkUserType =
        [targetUser](Qn::Permissions permissions)
        {
            switch (targetUser->userType())
            {
                case nx::vms::api::UserType::ldap:
                {
                    return permissions & ~(
                        Qn::WriteNamePermission
                        | Qn::WritePasswordPermission
                        | Qn::WriteEmailPermission
                    );
                }
                case nx::vms::api::UserType::cloud:
                {
                    return permissions & ~(
                        Qn::WritePasswordPermission
                        | Qn::WriteDigestPermission
                        | Qn::WriteEmailPermission
                        | Qn::WriteFullNamePermission
                    );
                }
                default:
                    return permissions;
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
    else
    {
        if (globalPermissions.testFlag(GlobalPermission::admin))
        {
            result |= Qn::ReadPermission;

            /* Nobody can do something with owner (except digest mode change). */
            if (targetUser->isOwner())
            {
                if (isSubjectOwner && !targetUser->isCloud())
                    result |= Qn::ReadWriteSavePermission | Qn::WriteDigestPermission;

                return result;
            }

            /* Only users may have admin permissions. */
            NX_ASSERT(subject.user());

            /* Admins can only be edited by owner, other users - by all admins. */
            bool targetIsAdmin = targetUser->getRawPermissions().testFlag(GlobalPermission::admin);
            if (isSubjectOwner || !targetIsAdmin)
                result |= Qn::FullUserPermissions;
        }
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
        return hasGlobalPermission(subject, GlobalPermission::admin);

    // Video wall admins can create layouts on video wall.
    if (resourcePool->getResourceById<QnVideoWallResource>(data.parentId))
        return hasGlobalPermission(subject, GlobalPermission::controlVideowall);

    // Tour owner can create layouts in it.
    const auto tour = m_context->layoutTourManager()->tour(data.parentId);
    if (tour.isValid())
        return tour.parentId == subject.id();

    const auto ownerResource = resourcePool->getResourceById(data.parentId);

    const auto owner = ownerResource.dynamicCast<QnUserResource>();
    if (!owner)
    {
        const auto parentCamera = ownerResource.dynamicCast<QnVirtualCameraResource>();
        if (parentCamera) //< Intercom layout case.
            return hasGlobalPermission(subject, GlobalPermission::userInput);

        return false;
    }

    // We can create layout for user if we can modify this user.
    return hasPermission(subject, owner, Qn::SavePermission);
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
    if (!m_context->userRolesManager()->hasRoles(data.userRoleIds))
        return false;

    if (data.isCloud && !data.fullName.isEmpty())
        return false; //< Full name for cloud users is allowed to modify via cloud portal only.

    return canCreateUser(subject, data.permissions, data.isAdmin);
}

bool QnResourceAccessManager::canCreateUser(const QnResourceAccessSubject& subject,
    GlobalPermissions targetPermissions, bool isOwner) const
{
    if (!subject.isValid())
        return false;

    /* Nobody can create owners. */
    if (isOwner)
        return false;

    /* Only owner can create admins. */
    if (targetPermissions.testFlag(GlobalPermission::admin))
        return subject.user() && subject.user()->isOwner();

    /* Admins can create other users. */
    return hasGlobalPermission(subject, GlobalPermission::admin);
}

bool QnResourceAccessManager::canCreateUser(const QnResourceAccessSubject& subject,
    Qn::UserRole role) const
{
    const auto permissions = QnUserRolesManager::userRolePermissions(role);
    const bool isOwner = (role == Qn::UserRole::owner);
    return canCreateUser(subject, permissions, isOwner);
}

bool QnResourceAccessManager::canModifyUser(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& target,
    const nx::vms::api::UserData& update) const
{
    if (!m_context->userRolesManager()->hasRoles(update.userRoleIds))
        return false;

    auto userResource = target.dynamicCast<QnUserResource>();
    NX_ASSERT(userResource);

    if (!subject.isValid() || !target)
        return false;

    /* Nobody can make user an owner (isAdmin ec2 field) unless target user is already an owner. */
    if (!userResource->isOwner() && update.isAdmin)
        return false;

    if (isPermissionsUpdateAttempted(userResource, update.permissions))
    {
        auto subjectUser = subject.user();
        return subjectUser && canUpdatePermissions(subjectUser, update);
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

    if (userResource->getRawPermissions() != update.permissions
        || userResource->isEnabled() != update.isEnabled)
    {
        requiredPermissions |= Qn::WriteAccessRightsPermission;
    }

    if (userResource->getEmail() != update.email)
        requiredPermissions |= Qn::WriteEmailPermission;

    if (userResource->fullName() != update.fullName)
        requiredPermissions |= Qn::WriteFullNamePermission;

    return hasPermission(subject, target, requiredPermissions);
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
    return hasGlobalPermission(subject, GlobalPermission::admin);
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
        return hasGlobalPermission(subject, GlobalPermission::admin);

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
    return hasGlobalPermission(subject, GlobalPermission::admin);
}

bool QnResourceAccessManager::hasAccessToAllCameras(
    const Qn::UserAccessData& userAccessData,
    QnResourcePool* resourcePool) const
{
    auto user =
        resourcePool->getResourceById<QnUserResource>(userAccessData.userId);
    if (!user)
        return false;

    if (user->isOwner())
        return true;

    const auto permissions = globalPermissions(user);
    return permissions & (GlobalPermission::admin | GlobalPermission::accessAllMedia);
}

