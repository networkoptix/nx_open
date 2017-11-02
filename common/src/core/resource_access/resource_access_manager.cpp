#include "resource_access_manager.h"

#include <common/common_module.h>

#include <nx/core/access/access_types.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_management/layout_tour_manager.h>

#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/resource_access_subjects_cache.h>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/webpage_resource.h>

#include <nx/streaming/abstract_archive_resource.h>

#include <nx_ec/data/api_layout_data.h>
#include <nx_ec/data/api_media_server_data.h>
#include <nx_ec/data/api_user_data.h>
#include <nx_ec/data/api_videowall_data.h>
#include <nx_ec/data/api_webpage_data.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/raii_guard.h>

#include <nx/fusion/model_functions.h>

#include <utils/common/scoped_timer.h>

using namespace nx::core::access;

QnResourceAccessManager::QnResourceAccessManager(Mode mode, QObject* parent /*= nullptr*/) :
    base_type(parent),
    QnUpdatable(),
    QnCommonModuleAware(parent),
    m_mode(mode),
    m_mutex(QnMutex::NonRecursive),
    m_permissionsCache()
{
    if (m_mode == Mode::cached)
    {
        const auto& resPool = commonModule()->resourcePool();

        /* This change affects all accessible resources. */
        connect(commonModule(), &QnCommonModule::readOnlyChanged, this,
            &QnResourceAccessManager::recalculateAllPermissions);

        connect(commonModule()->resourceAccessProvider(),
            &QnResourceAccessProvider::accessChanged,
            this,
            &QnResourceAccessManager::updatePermissions);

        connect(globalPermissionsManager(),
            &QnGlobalPermissionsManager::globalPermissionsChanged,
            this,
            &QnResourceAccessManager::updatePermissionsBySubject);

        connect(resPool, &QnResourcePool::resourceAdded, this,
            &QnResourceAccessManager::handleResourceAdded);
        connect(resPool, &QnResourcePool::resourceRemoved, this,
            &QnResourceAccessManager::handleResourceRemoved);

        connect(userRolesManager(), &QnUserRolesManager::userRoleRemoved, this,
            &QnResourceAccessManager::handleSubjectRemoved);

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

            if (subject.user())
                return subject.user()->resourcePool() != nullptr;

            return userRolesManager()->hasRole(subject.role().id);
        };

    PermissionKey key(subject.id(), resource->getId());
    if (isValid())
    {
        /* Store permissions in cache */
        QnMutexLocker lk(&m_mutex);
        auto& value = m_permissionsCache[key];
        if (value == permissions)
            return;
        value = permissions;
    }
    else
    {
        /* Just check if need to send the signal */
        QnMutexLocker lk(&m_mutex);
        if (m_permissionsCache.value(key) == permissions)
            return;
    }

    NX_LOG(QnLog::PERMISSIONS_LOG,
        lit("QnResourceAccessManager: %1 -> %2: %3")
        .arg(subject.name())
        .arg(resource->getName())
        .arg(QnLexical::serialized(permissions)),
        cl_logDEBUG1);
    emit permissionsChanged(subject, resource, permissions);
}

Qn::GlobalPermissions QnResourceAccessManager::globalPermissions(
    const QnResourceAccessSubject& subject) const
{
    return globalPermissionsManager()->globalPermissions(subject);
}

bool QnResourceAccessManager::hasGlobalPermission(
    const Qn::UserAccessData& accessRights,
    Qn::GlobalPermission requiredPermission) const
{
    return globalPermissionsManager()->hasGlobalPermission(accessRights, requiredPermission);
}

bool QnResourceAccessManager::hasGlobalPermission(
    const QnResourceAccessSubject& subject,
    Qn::GlobalPermission requiredPermission) const
{
    return globalPermissionsManager()->hasGlobalPermission(subject, requiredPermission);
}

Qn::Permissions QnResourceAccessManager::permissions(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    if (!subject.isValid() || !resource)
        return Qn::NoPermissions;

    /* User is already removed. */
    if (subject.user() && !subject.user()->resourcePool())
        return Qn::NoPermissions;

    /* Resource is not added to pool, checking if we can create such resource. */
    if (!resource->resourcePool())
        return canCreateResource(subject, resource)
            ? Qn::ReadWriteSavePermission
            : Qn::NoPermissions;

    if (m_mode == Mode::cached)
    {
        PermissionKey key(subject.id(), resource->getId());
        {
            QnMutexLocker lk(&m_mutex);
            auto iter = m_permissionsCache.find(key);
            if (iter != m_permissionsCache.cend())
                return *iter;
        }
    }

    return calculatePermissions(subject, resource);
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

    const auto& resPool = commonModule()->resourcePool();
    auto userResource = resPool->getResourceById<QnUserResource>(accessRights.userId);
    if (!userResource)
        return false;

    return hasPermission(userResource, resource, permissions);
}

bool QnResourceAccessManager::canCreateResource(const QnResourceAccessSubject& subject,
    const QnResourcePtr& target) const
{
    NX_ASSERT(subject.isValid());
    NX_ASSERT(target);
    NX_EXPECT(!isUpdating());

    if (!subject.isValid())
        return false;

    if (commonModule()->isReadOnly())
        return false;

    /* Check new layouts creating. */
    if (QnLayoutResourcePtr layout = target.dynamicCast<QnLayoutResource>())
        return canCreateLayout(subject, layout->getParentId());

    /* Check new users creating. */
    if (QnUserResourcePtr targetUser = target.dynamicCast<QnUserResource>())
        return canCreateUser(subject, targetUser->getRawPermissions(), targetUser->isOwner());

    if (QnStorageResourcePtr storage = target.dynamicCast<QnStorageResource>())
        return canCreateStorage(subject, storage->getParentId());

    if (QnVideoWallResourcePtr videoWall = target.dynamicCast<QnVideoWallResource>())
        return canCreateVideoWall(subject);

    if (QnWebPageResourcePtr webPage = target.dynamicCast<QnWebPageResource>())
        return canCreateWebPage(subject);

    return hasGlobalPermission(subject, Qn::GlobalAdminPermission);
}

bool QnResourceAccessManager::canCreateResource(const QnResourceAccessSubject& subject,
    const ec2::ApiStorageData& data) const
{
    NX_EXPECT(!isUpdating());
    return canCreateStorage(subject, data.parentId);
}

bool QnResourceAccessManager::canCreateResource(const QnResourceAccessSubject& subject,
    const ec2::ApiLayoutData& data) const
{
    NX_EXPECT(!isUpdating());
    return canCreateLayout(subject, data.parentId);
}

bool QnResourceAccessManager::canCreateResource(const QnResourceAccessSubject& subject,
    const ec2::ApiUserData& data) const
{
    NX_EXPECT(!isUpdating());
    if (!data.userRoleId.isNull() && !userRolesManager()->hasRole(data.userRoleId))
        return false;

    return canCreateUser(subject, data.permissions, data.isAdmin);
}

bool QnResourceAccessManager::canCreateResource(const QnResourceAccessSubject& subject,
    const ec2::ApiVideowallData& /*data*/) const
{
    NX_EXPECT(!isUpdating());
    return canCreateVideoWall(subject);
}

bool QnResourceAccessManager::canCreateResource(const QnResourceAccessSubject& subject,
    const ec2::ApiWebPageData& /*data*/) const
{
    NX_EXPECT(!isUpdating());
    return canCreateWebPage(subject);
}

void QnResourceAccessManager::beforeUpdate()
{
    if (m_mode == Mode::direct)
        return;

    /* We must clear cache here because otherwise there can stay resources, which were removed
     * during update. */
    QnMutexLocker lk(&m_mutex);
    m_permissionsCache.clear();
}

void QnResourceAccessManager::afterUpdate()
{
    if (m_mode == Mode::direct)
        return;

    recalculateAllPermissions();
}

void QnResourceAccessManager::recalculateAllPermissions()
{
    NX_EXPECT(m_mode == Mode::cached);
    QN_LOG_TIME(Q_FUNC_INFO);

    if (isUpdating())
        return;

    for (const auto& subject : resourceAccessSubjectsCache()->allSubjects())
        updatePermissionsBySubject(subject);
}

void QnResourceAccessManager::updatePermissions(const QnResourceAccessSubject& subject,
    const QnResourcePtr& target)
{
    NX_EXPECT(m_mode == Mode::cached);

    if (isUpdating())
        return;

    setPermissionsInternal(subject, target, calculatePermissions(subject, target));
}

void QnResourceAccessManager::updatePermissionsToResource(const QnResourcePtr& resource)
{
    if (isUpdating())
        return;

    for (const auto& subject : resourceAccessSubjectsCache()->allSubjects())
        updatePermissions(subject, resource);
}

void QnResourceAccessManager::updatePermissionsBySubject(const QnResourceAccessSubject& subject)
{
    if (isUpdating())
        return;

    const auto& resPool = commonModule()->resourcePool();
    for (const QnResourcePtr& resource : resPool->getResources())
        updatePermissions(subject, resource);
}

void QnResourceAccessManager::handleResourceAdded(const QnResourcePtr& resource)
{
    if (const auto& layout = resource.dynamicCast<QnLayoutResource>())
    {
        /* If layout become shared AND user is admin - he will not receive access notification
         * (because he already had access) but permissions must be recalculated. */
         connect(layout, &QnResource::parentIdChanged, this,
             &QnResourceAccessManager::updatePermissionsToResource);
        connect(layout, &QnLayoutResource::lockedChanged, this,
            &QnResourceAccessManager::updatePermissionsToResource);
    }

    if (const auto& camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        connect(camera, &QnVirtualCameraResource::initializedChanged, this,
            &QnResourceAccessManager::updatePermissionsToResource);

        connect(camera, &QnVirtualCameraResource::scheduleDisabledChanged, this,
            &QnResourceAccessManager::updatePermissionsToResource);

        connect(camera, &QnVirtualCameraResource::licenseUsedChanged, this,
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

    auto resourceId = resource->getId();

    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
        handleSubjectRemoved(user);

    for (const auto& subject : resourceAccessSubjectsCache()->allSubjects())
    {
        PermissionKey key(subject.id(), resourceId);
        {
            QnMutexLocker lk(&m_mutex);
            auto iter = m_permissionsCache.find(key);
            if (iter == m_permissionsCache.cend())
                continue;
            m_permissionsCache.erase(iter);
        }
        emit permissionsChanged(subject, resource, Qn::NoPermissions);
    }
}

void QnResourceAccessManager::handleSubjectRemoved(const QnResourceAccessSubject& subject)
{
    if (isUpdating())
        return;

    auto id = subject.id();
    const auto& resPool = commonModule()->resourcePool();
    QSet<QnResourcePtr> resorces;
    {
        QnMutexLocker lk(&m_mutex);
        auto iter = m_permissionsCache.begin();
        while (iter != m_permissionsCache.end())
        {
            auto key = iter.key();
            if (key.subjectId != id)
            {
                ++iter;
            }
            else
            {
                auto value = *iter;
                iter = m_permissionsCache.erase(iter);
                if (value == Qn::NoPermissions)
                    continue;

                QnResourcePtr targetResource = resPool->getResourceById(key.resourceId);
                if (targetResource)
                    resorces << targetResource;
            }
        }
    }
    for (const auto& targetResource : resorces)
        emit permissionsChanged(subject, targetResource, Qn::NoPermissions);
}

Qn::Permissions QnResourceAccessManager::calculatePermissions(
    const QnResourceAccessSubject& subject, const QnResourcePtr& target) const
{
    NX_ASSERT(target);
    if (subject.user() && !subject.user()->isEnabled())
        return Qn::NoPermissions;

    if (!target || !target->resourcePool())
        return Qn::NoPermissions;

    if (QnUserResourcePtr targetUser = target.dynamicCast<QnUserResource>())
        return calculatePermissionsInternal(subject, targetUser);

    if (QnLayoutResourcePtr layout = target.dynamicCast<QnLayoutResource>())
        return calculatePermissionsInternal(subject, layout);

    if (QnVirtualCameraResourcePtr camera = target.dynamicCast<QnVirtualCameraResource>())
        return calculatePermissionsInternal(subject, camera);

    if (QnMediaServerResourcePtr server = target.dynamicCast<QnMediaServerResource>())
        return calculatePermissionsInternal(subject, server);

    if (QnStorageResourcePtr storage = target.dynamicCast<QnStorageResource>())
        return calculatePermissionsInternal(subject, storage);

    if (QnVideoWallResourcePtr videoWall = target.dynamicCast<QnVideoWallResource>())
        return calculatePermissionsInternal(subject, videoWall);

    if (QnWebPageResourcePtr webPage = target.dynamicCast<QnWebPageResource>())
        return calculatePermissionsInternal(subject, webPage);

    if (QnAbstractArchiveResourcePtr archive = target.dynamicCast<QnAbstractArchiveResource>())
        return Qn::ReadPermission | Qn::ExportPermission;

    NX_ASSERT(false, "invalid resource type");
    return Qn::NoPermissions;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject,
    const QnVirtualCameraResourcePtr& camera) const
{
    NX_ASSERT(camera);

    Qn::Permissions result = Qn::NoPermissions;

    /* Admins must be able to remove any cameras to delete servers.  */
    if (hasGlobalPermission(subject, Qn::GlobalAdminPermission))
        result |= Qn::RemovePermission;

    if (!commonModule()->resourceAccessProvider()->hasAccess(subject, camera))
        return result;

    result |= Qn::ReadPermission | Qn::ViewContentPermission;

    bool liveAllowed = true;
    bool footageAllowed = hasGlobalPermission(subject, Qn::GlobalViewArchivePermission);
    bool exportAllowed = hasGlobalPermission(subject, Qn::GlobalExportPermission);

    if (!camera->isLicenseUsed())
    {
        switch (camera->licenseType())
        {
            case Qn::LC_Bridge:
            {
                footageAllowed = false;
                exportAllowed = false;
                break;
            }

            // TODO: Forbid all for VMAX when discussed with management
            case Qn::LC_VMAX:
            {
                //liveAllowed = false;
                //footageAllowed = false;
                break;
            }
            default:
                break;
        }
    }

    if (liveAllowed)
        result |= Qn::ViewLivePermission;

    if (footageAllowed)
        result |= Qn::ViewFootagePermission;

    if (exportAllowed)
    {
        NX_EXPECT(footageAllowed, "Server API cannot allow export without footage access.");
        result |= Qn::ExportPermission;
    }

    if (hasGlobalPermission(subject, Qn::GlobalUserInputPermission))
        result |= Qn::WritePtzPermission;

    if (commonModule()->isReadOnly())
        return result;

    if (hasGlobalPermission(subject, Qn::GlobalEditCamerasPermission))
        result |= Qn::ReadWriteSavePermission | Qn::WriteNamePermission;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnMediaServerResourcePtr& server) const
{
    NX_ASSERT(server);

    Qn::Permissions result = Qn::ReadPermission;
    if (!commonModule()->resourceAccessProvider()->hasAccess(subject, server))
        return result;

    result |= Qn::ViewContentPermission;
    if (hasGlobalPermission(subject, Qn::GlobalAdminPermission) && !commonModule()->isReadOnly())
        result |= Qn::FullServerPermissions;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnStorageResourcePtr& storage) const
{
    NX_ASSERT(storage);
    auto server = storage->getParentServer();
    if (!server)
        return Qn::ReadWriteSavePermission | Qn::RemovePermission;  /*< Server is already deleted. Storage can be removed. */

    auto serverPermissions = permissions(subject, server);
    if (serverPermissions.testFlag(Qn::RemovePermission))
        return Qn::ReadWriteSavePermission | Qn::RemovePermission;

    if (serverPermissions.testFlag(Qn::SavePermission))
        return Qn::ReadWriteSavePermission;

    return Qn::NoPermissions;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnVideoWallResourcePtr& videoWall) const
{
    NX_ASSERT(videoWall);
    Qn::Permissions result = Qn::NoPermissions;
    if (!hasGlobalPermission(subject, Qn::GlobalControlVideoWallPermission))
        return result;

    result = Qn::ReadPermission | Qn::WritePermission;
    if (commonModule()->isReadOnly())
        return result;

    result |= Qn::SavePermission | Qn::WriteNamePermission;
    if (hasGlobalPermission(subject, Qn::GlobalAdminPermission))
        result |= Qn::RemovePermission;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnWebPageResourcePtr& webPage) const
{
    NX_ASSERT(webPage);

    if (!commonModule()->resourceAccessProvider()->hasAccess(subject, webPage))
        return Qn::NoPermissions;

    Qn::Permissions result = Qn::ReadPermission | Qn::ViewContentPermission;
    if (commonModule()->isReadOnly())
        return result;

    /* Web Page behaves totally like camera. */
    if (hasGlobalPermission(subject, Qn::GlobalEditCamerasPermission))
        result |= Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnLayoutResourcePtr& layout) const
{
    NX_ASSERT(layout);
    if (!subject.isValid() || !layout)
        return Qn::NoPermissions;

    QnUuid ownerId = layout->getParentId();

    /* Access to videowall layouts. */
    const auto videowall = resourcePool()->getResourceById<QnVideoWallResource>(ownerId);

    // TODO: #GDM Code duplication with QnWorkbenchAccessController::calculatePermissionsInternal
    auto checkReadOnly =
        [this](Qn::Permissions permissions)
        {
            if (!commonModule()->isReadOnly())
                return permissions;
            return permissions& ~(Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission);
        };

    auto checkLocked =
        [this, layout](const Qn::Permissions permissions)
        {
            if (!layout->locked())
                return permissions;

            Qn::Permissions forbidden = Qn::AddRemoveItemsPermission | Qn::WriteNamePermission;

            // Autogenerated layouts (e.g. videowall) can be removed even when locked
            if (!layout->isServiceLayout())
                forbidden |= Qn::RemovePermission;

            return permissions & ~forbidden;
        };

    /* Layouts with desktop cameras are not to be modified but can be removed. */
    const auto items = layout->getItems();
    for (auto item: items)
    {
        const auto resourceId = item.resource.id;
        if (resourceId.isNull())
            continue;

        if (const auto& resource = resourcePool()->getResourceById(resourceId))
        {
            if (resource->hasFlags(Qn::desktop_camera))
                return checkReadOnly(Qn::ReadPermission | Qn::RemovePermission);
        }
    }

    /* Calculate base layout permissions */
    auto base = [&]() -> Qn::Permissions
        {
            /* Owner can do anything. */
            if (subject.user() && subject.user()->isOwner())
                return Qn::FullLayoutPermissions;


            if (videowall)
            {
                /* Videowall layout. */
                return hasGlobalPermission(subject, Qn::GlobalControlVideoWallPermission)
                    ? Qn::FullLayoutPermissions
                    : Qn::NoPermissions;
            }


            /* Access to global layouts. Simple check is enough, exported layouts are checked on the client side. */
            if (layout->isShared())
            {
                if (!commonModule()->resourceAccessProvider()->hasAccess(subject, layout))
                    return Qn::NoPermissions;

                /* Global layouts editor. */
                if (hasGlobalPermission(subject, Qn::GlobalAdminPermission))
                    return Qn::FullLayoutPermissions;

                return Qn::ModifyLayoutPermission;
            }

            /* Checking other user's layout. Only users can have access to them. */
            if (auto user = subject.user())
            {

                if (ownerId != user->getId())
                {
                    QnUserResourcePtr owner = resourcePool()->getResourceById<QnUserResource>(ownerId);

                    if (!owner)
                    {
                        /* Everybody can modify lite client layout. */
                        const auto server = resourcePool()->getResourceById<QnMediaServerResource>(ownerId);
                        if (server)
                            return Qn::FullLayoutPermissions;

                        const auto tour = commonModule()->layoutTourManager()->tour(ownerId);
                        if (tour.isValid())
                        {
                            return tour.parentId == user->getId()
                                ? Qn::FullLayoutPermissions
                                : Qn::NoPermissions;
                        }

                        /* Layout of user, which we don't know of. */
                        return hasGlobalPermission(subject, Qn::GlobalAdminPermission)
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

    return checkLocked(checkReadOnly(base()));
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnUserResourcePtr& targetUser) const
{
    NX_ASSERT(targetUser);

    auto checkReadOnly = [this](Qn::Permissions permissions)
    {
        if (!commonModule()->isReadOnly())
            return permissions;
        return permissions &~ (Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission
            | Qn::WritePasswordPermission | Qn::WriteEmailPermission | Qn::WriteFullNamePermission);
    };

    auto checkUserType = [targetUser](Qn::Permissions permissions)
    {
        switch (targetUser->userType())
        {
        case QnUserType::Ldap:
            return permissions &~ (Qn::WriteNamePermission | Qn::WritePasswordPermission | Qn::WriteEmailPermission);
        case QnUserType::Cloud:
            return permissions &~ (Qn::WritePasswordPermission | Qn::WriteEmailPermission | Qn::WriteFullNamePermission);
        default:
            break;
        }
        return permissions;
    };

    Qn::Permissions result = Qn::NoPermissions;
    if (targetUser == subject.user())
    {
        /* Everyone can edit own data. */
        result |= Qn::ReadWriteSavePermission | Qn::WritePasswordPermission
            | Qn::WriteEmailPermission | Qn::WriteFullNamePermission;
    }
    else
    {
        if (hasGlobalPermission(subject, Qn::GlobalAdminPermission))
        {
            result |= Qn::ReadPermission;

            /* Nobody can do something with owner. */
            if (targetUser->isOwner())
                return result;

            /* Only users may have admin permissions. */
            NX_ASSERT(subject.user());
            auto isOwner = subject.user() && subject.user()->isOwner();

            /* Admins can only be edited by owner, other users - by all admins. */
            bool targetIsAdmin = targetUser->getRawPermissions().testFlag(Qn::GlobalAdminPermission);
            if (isOwner || !targetIsAdmin)
                result |= Qn::FullUserPermissions;
        }
    }

    return checkReadOnly(checkUserType(result));
}

bool QnResourceAccessManager::canCreateStorage(const QnResourceAccessSubject& subject,
    const QnUuid& storageParentId) const
{
    if (!subject.isValid() || commonModule()->isReadOnly())
        return false;

    const auto& resPool = commonModule()->resourcePool();
    auto server = resPool->getResourceById<QnMediaServerResource>(storageParentId);
    return hasPermission(subject, server, Qn::SavePermission);
}

bool QnResourceAccessManager::canCreateLayout(const QnResourceAccessSubject& subject,
    const QnUuid& layoutParentId) const
{
    const auto& resPool = commonModule()->resourcePool();
    if (!subject.isValid() || commonModule()->isReadOnly())
        return false;

    /* Everybody can create own layouts. */
    if (subject.user() && layoutParentId == subject.user()->getId())
        return true;

    /* Only admins can create global layouts. */
    if (layoutParentId.isNull())
        return hasGlobalPermission(subject, Qn::GlobalAdminPermission);

    /* Videowall-admins can create layouts on videowall. */
    if (resPool->getResourceById<QnVideoWallResource>(layoutParentId))
        return hasGlobalPermission(subject, Qn::GlobalControlVideoWallPermission);

    // Tour owner can create layouts in it.
    const auto tour = layoutTourManager()->tour(layoutParentId);
    if (tour.isValid())
        return tour.parentId == subject.id();

    const auto ownerResource = resPool->getResourceById(layoutParentId);

    /* Everybody can create layout for lite client. */
    if (const auto& parentServer = ownerResource.dynamicCast<QnMediaServerResource>())
    {
        if (!parentServer->getServerFlags().testFlag(Qn::SF_HasLiteClient))
            return false;

        /* There can only be one layout for each lite client. */
        const auto existingLayouts = resPool->getResources<QnLayoutResource>().filtered(
            [id = parentServer->getId()](const QnLayoutResourcePtr& layout)
            {
                return layout->getId() == id;
            });

        return existingLayouts.isEmpty();
    }

    const auto owner = ownerResource.dynamicCast<QnUserResource>();
    if (!owner)
        return false;

    /* We can create layout for user if we can modify this user. */
    return hasPermission(subject, owner, Qn::SavePermission);
}

bool QnResourceAccessManager::canCreateUser(const QnResourceAccessSubject& subject,
    Qn::GlobalPermissions targetPermissions, bool isOwner) const
{
    if (!subject.isValid() || commonModule()->isReadOnly())
        return false;

    /* Nobody can create owners. */
    if (isOwner)
        return false;

    /* Only owner can create admins. */
    if (targetPermissions.testFlag(Qn::GlobalAdminPermission))
        return subject.user() && subject.user()->isOwner();

    /* Admins can create other users. */
    return hasGlobalPermission(subject, Qn::GlobalAdminPermission);
}

bool QnResourceAccessManager::canCreateUser(const QnResourceAccessSubject& subject,
    Qn::UserRole role) const
{
    const auto permissions = QnUserRolesManager::userRolePermissions(role);
    const bool isOwner = (role == Qn::UserRole::Owner);
    return canCreateUser(subject, permissions, isOwner);
}

bool QnResourceAccessManager::canCreateVideoWall(const QnResourceAccessSubject& subject) const
{
    if (!subject.isValid() || commonModule()->isReadOnly())
        return false;

    /* Only admins can create new videowalls (and attach new screens). */
    return hasGlobalPermission(subject, Qn::GlobalAdminPermission);
}

bool QnResourceAccessManager::canCreateWebPage(const QnResourceAccessSubject& subject) const
{
    if (!subject.isValid() || commonModule()->isReadOnly())
        return false;

    /* Only admins can add new web pages. */
    return hasGlobalPermission(subject, Qn::GlobalAdminPermission);
}

bool QnResourceAccessManager::canModifyResource(const QnResourceAccessSubject& subject,
    const QnResourcePtr& target, const ec2::ApiStorageData& update) const
{
    NX_ASSERT(target.dynamicCast<QnStorageResource>());

    // Storages cannot be moved from one server to another.
    // Null parentId is allowed because this check is performed before possible API Merge.
    if (!update.parentId.isNull() && target->getParentId() != update.parentId)
        return false;

    /* Otherwise - default behavior. */
    return hasPermission(subject, target, Qn::SavePermission);
}

bool QnResourceAccessManager::canModifyResource(const QnResourceAccessSubject& subject,
    const QnResourcePtr& target, const ec2::ApiLayoutData& update) const
{
    NX_ASSERT(target.dynamicCast<QnLayoutResource>());

    /* If we are changing layout parent, it is equal to creating new layout. */
    if (target->getParentId() != update.parentId)
        return canCreateLayout(subject, update.parentId);

    /* Otherwise - default behavior. */
    return hasPermission(subject, target, Qn::SavePermission);
}

bool QnResourceAccessManager::canModifyResource(const QnResourceAccessSubject& subject,
    const QnResourcePtr& target, const ec2::ApiUserData& update) const
{
    if (!update.userRoleId.isNull() && !userRolesManager()->hasRole(update.userRoleId))
        return false;

    auto userResource = target.dynamicCast<QnUserResource>();
    NX_ASSERT(userResource);

    if (!subject.isValid() || !target)
        return false;

    /* Nobody can make user an owner (isAdmin ec2 field) unless target user is already an owner. */
    if (!userResource->isOwner() && update.isAdmin)
        return false;

    /* We should have full access to user to modify him. */
    Qn::Permissions requiredPermissions = Qn::ReadWriteSavePermission;

    if (target->getName() != update.name)
        requiredPermissions |= Qn::WriteNamePermission;

    if (userResource->getHash() != update.hash)
        requiredPermissions |= Qn::WritePasswordPermission;

    // TODO: #rvasilenko describe somewhere password changing logic, it looks like hell to me
    if (userResource->getDigest() != update.digest)
        requiredPermissions |= Qn::WritePasswordPermission;

    if (userResource->getRawPermissions() != update.permissions)
        requiredPermissions |= Qn::WriteAccessRightsPermission;

    if (userResource->getEmail() != update.email)
        requiredPermissions |= Qn::WriteEmailPermission;

    if (userResource->fullName() != update.fullName)
        requiredPermissions |= Qn::WriteFullNamePermission;

    return hasPermission(subject, target, requiredPermissions);
}

bool QnResourceAccessManager::canModifyResource(const QnResourceAccessSubject& subject,
    const QnResourcePtr& target, const ec2::ApiVideowallData& update) const
{
    if (!subject.isValid() || commonModule()->isReadOnly())
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
        return hasGlobalPermission(subject, Qn::GlobalAdminPermission);

    /* Otherwise - default behavior. */
    return hasPermission(subject, target, Qn::SavePermission);
}
