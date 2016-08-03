#include "resource_access_manager.h"

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/webpage_resource.h>

#include <nx_ec/data/api_layout_data.h>
#include <nx_ec/data/api_media_server_data.h>
#include <nx_ec/data/api_user_data.h>
#include <nx_ec/data/api_videowall_data.h>
#include <nx_ec/data/api_webpage_data.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/raii_guard.h>

//#define DEBUG_PERMISSIONS
#ifdef DEBUG_PERMISSIONS
#define TRACE(...) qDebug() << "QnResourceAccessManager: " << __VA_ARGS__;
#else
#define TRACE(...)
#endif

class QnResourceAccessManager::UpdateCacheGuard : QnRaiiGuard
{
public:
    UpdateCacheGuard(QnResourceAccessManager* parent) : QnRaiiGuard(
        [parent]() { parent->beginUpdateCache(); },
        [parent]() { parent->endUpdateCache(); }) {}
};

QnResourceAccessManager::QnResourceAccessManager(QObject* parent /*= nullptr*/) :
    base_type(parent),
    m_mutex(QnMutex::NonRecursive),
    m_accessibleResources(),
    m_userGroups(),
    m_globalPermissionsCache(),
    m_cacheUpdateCounter(0),
    m_invalidatedResources(),
    m_permissionsCache()
{
    /* This change affects all accessible resources. */
    connect(qnCommon,& QnCommonModule::readOnlyChanged, this, [this](bool readOnly)
    {
        UpdateCacheGuard guard(this);
        {
            Q_UNUSED(readOnly);
            QnMutexLocker lk(&m_mutex);
            m_permissionsCache.clear();
        }
    });

    auto handleResourceAdded = [this]
        (const QnResourcePtr& resource)
    {
        if (const auto& layout = resource.dynamicCast<QnLayoutResource>())
        {
            /* To make layouts global */
            connect(layout, &QnResource::parentIdChanged,       this, &QnResourceAccessManager::invalidateResourceCache);
            connect(layout, &QnLayoutResource::lockedChanged,   this, &QnResourceAccessManager::invalidateResourceCache);

            /* Some cameras may become available to users - or vice versa. */
            connect(layout, &QnResource::parentIdChanged,       this, &QnResourceAccessManager::invalidateCacheForLayoutItems);
            connect(layout, &QnLayoutResource::itemAdded,       this, &QnResourceAccessManager::invalidateCacheForLayoutItem);
            connect(layout, &QnLayoutResource::itemChanged,     this, &QnResourceAccessManager::invalidateCacheForLayoutItem);
            connect(layout, &QnLayoutResource::itemRemoved,     this, &QnResourceAccessManager::invalidateCacheForLayoutItem);
        }

        if (const auto& user = resource.dynamicCast<QnUserResource>())
        {
            connect(user, &QnUserResource::permissionsChanged,  this, &QnResourceAccessManager::invalidateResourceCache);
            connect(user, &QnUserResource::userGroupChanged,    this, &QnResourceAccessManager::invalidateResourceCache);
            connect(user, &QnUserResource::enabledChanged,      this, &QnResourceAccessManager::invalidateResourceCache);
        }

        /* Storage can be added (and permissions requested) before the server. */
        if (const auto& server = resource.dynamicCast<QnMediaServerResource>())
        {
            UpdateCacheGuard guard(this);
            for (auto storage : server->getStorages())
                invalidateResourceCache(storage);
        }

        if (const auto& videowall = resource.dynamicCast<QnVideoWallResource>())
        {
            connect(videowall, &QnVideoWallResource::itemAdded,   this, &QnResourceAccessManager::invalidateCacheForVideowallItem);
            connect(videowall, &QnVideoWallResource::itemChanged, this, &QnResourceAccessManager::invalidateCacheForVideowallItem);
            connect(videowall, &QnVideoWallResource::itemRemoved, this, &QnResourceAccessManager::invalidateCacheForVideowallItem);
        }
    };

    auto handleResourceRemoved = [this](const QnResourcePtr& resource)
    {
        UpdateCacheGuard guard(this);
        disconnect(resource, nullptr, this, nullptr);
        invalidateResourceCache(resource);
        invalidateCacheForLayoutItems(resource);

        if (auto videowall = resource.dynamicCast<QnVideoWallResource>())
        {
            for (const auto& item : videowall->items()->getItems())
                invalidateCacheForVideowallItem(videowall, item);

            for (const QnResourcePtr& desktopCam : qnResPool->getResourcesWithFlag(Qn::desktop_camera))
                invalidateResourceCache(desktopCam);
        }
    };

    connect(qnResPool, &QnResourcePool::resourceAdded,   this, handleResourceAdded);
    connect(qnResPool, &QnResourcePool::resourceRemoved, this, handleResourceRemoved);

    for (const QnResourcePtr& resource : qnResPool->getResources())
        handleResourceAdded(resource);
}

Qn::GlobalPermissions QnResourceAccessManager::dependentPermissions(Qn::GlobalPermission value)
{
    switch (value)
    {
        case Qn::GlobalViewArchivePermission:
            return Qn::GlobalViewBookmarksPermission | Qn::GlobalExportPermission | Qn::GlobalManageBookmarksPermission;
        case Qn::GlobalViewBookmarksPermission:
            return Qn::GlobalManageBookmarksPermission;
        default:
            break;
    }
    return Qn::NoGlobalPermissions;

}

void QnResourceAccessManager::resetAccessibleResources(const ec2::ApiAccessRightsDataList& accessibleResourcesList)
{
    UpdateCacheGuard guard(this);
    {
        QnMutexLocker lk(&m_mutex);
        m_accessibleResources.clear();
        for (const auto& item : accessibleResourcesList)
        {
            QSet<QnUuid>& accessibleResources = m_accessibleResources[item.userId];
            for (const auto& id : item.resourceIds)
                accessibleResources << id;
        }
        m_permissionsCache.clear();
    }
}

ec2::ApiUserGroupDataList QnResourceAccessManager::userGroups() const
{
    QnMutexLocker lk(&m_mutex);
    ec2::ApiUserGroupDataList result;
    result.reserve(m_userGroups.size());
    for (const auto& group: m_userGroups)
        result.push_back(group);
    return result;
}

void QnResourceAccessManager::resetUserGroups(const ec2::ApiUserGroupDataList& userGroups)
{
    UpdateCacheGuard guard(this);
    {
        QnMutexLocker lk(&m_mutex);
        m_permissionsCache.clear();
        m_globalPermissionsCache.clear();
        m_userGroups.clear();
        for (const auto& group : userGroups)
            m_userGroups.insert(group.id, group);
    }
}

ec2::ApiUserGroupData QnResourceAccessManager::userGroup(const QnUuid& groupId) const
{
    QnMutexLocker lk(&m_mutex);
    return m_userGroups.value(groupId);
}

void QnResourceAccessManager::addOrUpdateUserGroup(const ec2::ApiUserGroupData& userGroup)
{
    {
        QnMutexLocker lk(&m_mutex);
        m_userGroups[userGroup.id] = userGroup;
    }

    for (const auto& user : qnResPool->getResources<QnUserResource>())
    {
        if (user->userGroup() == userGroup.id)
            invalidateResourceCache(user);
    }

    emit userGroupAddedOrUpdated(userGroup);
}

void QnResourceAccessManager::removeUserGroup(const QnUuid& groupId)
{
    {
        QnMutexLocker lk(&m_mutex);
        m_userGroups.remove(groupId);
    }

    for (const auto& user : qnResPool->getResources<QnUserResource>())
    {
        if (user->userGroup() == groupId)
            invalidateResourceCache(user);
    }

    emit userGroupRemoved(groupId);
}

QSet<QnUuid> QnResourceAccessManager::accessibleResources(const QnUuid& userOrGroupId) const
{
    QnMutexLocker lk(&m_mutex);
    return m_accessibleResources[userOrGroupId];
}

QSet<QnUuid> QnResourceAccessManager::accessibleResources(const QnUserResourcePtr& user) const
{
    QnUuid keyId = user->userGroup();
    if (keyId.isNull())
        keyId = user->getId();

    return accessibleResources(keyId);
}

void QnResourceAccessManager::setAccessibleResources(const QnUuid& userOrGroupId, const QSet<QnUuid>& resources)
{
    {
        UpdateCacheGuard guard(this);
        {
            QnMutexLocker lk(&m_mutex);
            if (m_accessibleResources[userOrGroupId] == resources)
                return;

            auto removed = m_accessibleResources[userOrGroupId] - resources;
            auto added = resources - m_accessibleResources[userOrGroupId];
            m_accessibleResources[userOrGroupId] = resources;

            m_invalidatedResources += removed;
            m_invalidatedResources += added;
        }

        invalidateResourceCacheInternal(userOrGroupId);
        for (const auto& user : qnResPool->getResources<QnUserResource>())
        {
            if (user->userGroup() == userOrGroupId)
                invalidateResourceCache(user);
        }
    }

    emit accessibleResourcesChanged(userOrGroupId);
}

Qn::GlobalPermissions QnResourceAccessManager::globalPermissions(const QnUserResourcePtr& user) const
{
    auto filterDependentPermissions = [](Qn::GlobalPermissions value)
    {
        //TODO: #GDM code duplication with ::dependentPermissions() method.
        Qn::GlobalPermissions result = value;
        if (!result.testFlag(Qn::GlobalViewArchivePermission))
        {
            result &= ~Qn::GlobalViewBookmarksPermission;
            result &= ~Qn::GlobalExportPermission;
        }

        if (!result.testFlag(Qn::GlobalViewBookmarksPermission))
        {
            result &= ~Qn::GlobalManageBookmarksPermission;
        }
        return result;
    };

    if (!user || !user->isEnabled())
        return Qn::NoGlobalPermissions;

    /* Handle just-created user situation. */
    if (user->flags().testFlag(Qn::local))
        return filterDependentPermissions(user->getRawPermissions());

    /* User is already removed. Problems with 'on_resource_removed' connection order. */
    if (!user->resourcePool())
        return Qn::NoGlobalPermissions;

    QnUuid userId = user->getId();

    {
        QnMutexLocker lk(&m_mutex);
        auto iter = m_globalPermissionsCache.find(userId);
        if (iter != m_globalPermissionsCache.cend())
            return *iter;
    }

    Qn::GlobalPermissions result;
    QnUuid groupId = user->userGroup();

    if (user->isOwner())
    {
        result = Qn::GlobalAdminPermissionSet;
    }
    else if (!groupId.isNull())
    {
        result = userGroup(groupId).permissions;    /*< If the group does not exist, permissions will be empty. */
        result &= ~Qn::GlobalAdminPermission;       /*< If user belongs to group, he cannot be an admin - by design. */
    }
    else
    {
        result = user->getRawPermissions();
        if (result.testFlag(Qn::GlobalAdminPermission))
            result |= Qn::GlobalAdminPermissionSet;
    }

    result = filterDependentPermissions(result);

    {
        QnMutexLocker lk(&m_mutex);
        m_globalPermissionsCache.insert(userId, result);
    }

    return result;
}

bool QnResourceAccessManager::hasGlobalPermission(const QnUserResourcePtr& user, Qn::GlobalPermission requiredPermission) const
{
    if (requiredPermission == Qn::NoGlobalPermissions)
        return true;

    return globalPermissions(user).testFlag(requiredPermission);
}

Qn::Permissions QnResourceAccessManager::permissions(const QnUserResourcePtr& user, const QnResourcePtr& resource) const
{
    if (!user || !resource)
        return Qn::NoPermissions;

    /* Resource is not added to pool, checking if we can create such resource. */
    if (!resource->resourcePool())
        return canCreateResource(user, resource)
            ? Qn::ReadWriteSavePermission
            : Qn::NoPermissions;

    PermissionKey key(user->getId(), resource->getId());

    {
        QnMutexLocker lk(&m_mutex);
        auto iter = m_permissionsCache.find(key);
        if (iter != m_permissionsCache.cend())
            return *iter;
    }

    Qn::Permissions result = calculatePermissions(user, resource);
    {
        QnMutexLocker lk(&m_mutex);
        m_permissionsCache.insert(key, result);
    }
    return result;
}

bool QnResourceAccessManager::hasPermission(const QnUserResourcePtr& user, const QnResourcePtr& resource, Qn::Permissions requiredPermissions) const
{
    return (permissions(user, resource) & requiredPermissions) == requiredPermissions;
}

bool QnResourceAccessManager::canCreateResource(const QnUserResourcePtr& user, const QnResourcePtr& target) const
{
    NX_ASSERT(user);
    NX_ASSERT(target);

    if (!user)
        return false;

    if (qnCommon->isReadOnly())
        return false;

    /* Check new layouts creating. */
    if (QnLayoutResourcePtr layout = target.dynamicCast<QnLayoutResource>())
        return canCreateLayout(user, layout->getParentId());

    /* Check new users creating. */
    if (QnUserResourcePtr targetUser = target.dynamicCast<QnUserResource>())
        return canCreateUser(user, targetUser->getRawPermissions(), targetUser->isOwner());

    if (QnStorageResourcePtr storage = target.dynamicCast<QnStorageResource>())
        return canCreateStorage(user, storage->getParentId());

    if (QnVideoWallResourcePtr videoWall = target.dynamicCast<QnVideoWallResource>())
        return canCreateVideoWall(user);

    if (QnWebPageResourcePtr webPage = target.dynamicCast<QnWebPageResource>())
        return canCreateWebPage(user);

    return hasGlobalPermission(user, Qn::GlobalAdminPermission);
}

bool QnResourceAccessManager::canCreateResource(const QnUserResourcePtr& user, const ec2::ApiStorageData& data) const
{
    return canCreateStorage(user, data.parentId);
}

bool QnResourceAccessManager::canCreateResource(const QnUserResourcePtr& user, const ec2::ApiLayoutData& data) const
{
    return canCreateLayout(user, data.parentId);
}

bool QnResourceAccessManager::canCreateResource(const QnUserResourcePtr& user, const ec2::ApiUserData& data) const
{
    return canCreateUser(user, data.permissions, data.isAdmin);
}

bool QnResourceAccessManager::canCreateResource(const QnUserResourcePtr& user, const ec2::ApiVideowallData& data) const
{
    Q_UNUSED(data);
    return canCreateVideoWall(user);
}

bool QnResourceAccessManager::canCreateResource(const QnUserResourcePtr& user, const ec2::ApiWebPageData& data) const
{
    Q_UNUSED(data);
    return canCreateWebPage(user);
}

void QnResourceAccessManager::invalidateResourceCache(const QnResourcePtr& resource)
{
    NX_ASSERT(resource);
    if (!resource)
        return;

    UpdateCacheGuard guard(this);
    invalidateResourceCacheInternal(resource->getId());
}

void QnResourceAccessManager::invalidateResourceCacheInternal(const QnUuid& resourceId)
{
    QnMutexLocker lk(&m_mutex);
    bool invalidated = (m_globalPermissionsCache.remove(resourceId) > 0);
    for (auto iter = m_permissionsCache.begin(); iter != m_permissionsCache.end();)
    {
        if (iter.key().userId == resourceId || iter.key().resourceId == resourceId)
        {
            iter = m_permissionsCache.erase(iter);
            invalidated = true;
        }
        else
        {
            ++iter;
        }
    }

    if (invalidated)
        m_invalidatedResources << resourceId;
}

void QnResourceAccessManager::invalidateCacheForLayoutItem(const QnLayoutResourcePtr& layout, const QnLayoutItemData& item)
{
    Q_UNUSED(layout);
    UpdateCacheGuard guard(this);
    invalidateResourceCacheInternal(item.resource.id);
}

void QnResourceAccessManager::invalidateCacheForLayoutItems(const QnResourcePtr& resource)
{
    UpdateCacheGuard guard(this);
    if (const QnLayoutResourcePtr& layout = resource.dynamicCast<QnLayoutResource>())
    {
        for (const auto& item : layout->getItems())
            invalidateCacheForLayoutItem(layout, item);
    }
}

void QnResourceAccessManager::invalidateCacheForVideowallItem(const QnVideoWallResourcePtr &resource, const QnVideoWallItem &item)
{
    Q_UNUSED(resource);
    UpdateCacheGuard guard(this);
    invalidateResourceCacheInternal(item.layout);
    invalidateCacheForLayoutItems(qnResPool->getResourceById(item.layout));
}

Qn::Permissions QnResourceAccessManager::calculatePermissions(const QnUserResourcePtr& user, const QnResourcePtr& target) const
{
    NX_ASSERT(target);
    if (!user->isEnabled())
        return Qn::NoPermissions;

    if (QnUserResourcePtr targetUser = target.dynamicCast<QnUserResource>())
        return calculatePermissionsInternal(user, targetUser);

    if (QnLayoutResourcePtr layout = target.dynamicCast<QnLayoutResource>())
        return calculatePermissionsInternal(user, layout);

    if (QnVirtualCameraResourcePtr camera = target.dynamicCast<QnVirtualCameraResource>())
        return calculatePermissionsInternal(user, camera);

    if (QnMediaServerResourcePtr server = target.dynamicCast<QnMediaServerResource>())
        return calculatePermissionsInternal(user, server);

    if (QnStorageResourcePtr storage = target.dynamicCast<QnStorageResource>())
        return calculatePermissionsInternal(user, storage);

    if (QnVideoWallResourcePtr videoWall = target.dynamicCast<QnVideoWallResource>())
        return calculatePermissionsInternal(user, videoWall);

    if (QnWebPageResourcePtr webPage = target.dynamicCast<QnWebPageResource>())
        return calculatePermissionsInternal(user, webPage);

    NX_ASSERT("invalid resource type");
    return Qn::NoPermissions;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(const QnUserResourcePtr& user, const QnVirtualCameraResourcePtr& camera) const
{
    TRACE("Calculate permissions of user " << user->getName() << " to camera " << camera->getName())
    NX_ASSERT(camera);
    Qn::Permissions result = Qn::NoPermissions;

    if (isAccessibleResource(user, camera) == Access::Forbidden)
        return result;

    result |= Qn::ReadPermission;
    if (hasGlobalPermission(user, Qn::GlobalExportPermission))
        result |= Qn::ExportPermission;

    if (hasGlobalPermission(user, Qn::GlobalUserInputPermission))
        result |= Qn::WritePtzPermission;

    if (qnCommon->isReadOnly())
        return result;

    if (hasGlobalPermission(user, Qn::GlobalEditCamerasPermission))
        result |= Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(const QnUserResourcePtr& user, const QnMediaServerResourcePtr& server) const
{
    NX_ASSERT(server);
    /* All users must have at least ReadPermission to send api requests
     * (recorded periods, bookmarks, etc) and view servers on shared layouts. */
    Qn::Permissions result = Qn::ReadPermission;

    if (hasGlobalPermission(user, Qn::GlobalAdminPermission) && !qnCommon->isReadOnly())
        result |= Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(const QnUserResourcePtr& user, const QnStorageResourcePtr& storage) const
{
    NX_ASSERT(storage);
    auto server = storage->getParentServer();
    if (!server)
        return Qn::ReadPermission;  /*< Server is not added to resource pool. Really we shouldn't request storage permissions in that case. */

    auto serverPermissions = permissions(user, server);
    if (serverPermissions.testFlag(Qn::SavePermission))
        return Qn::ReadWriteSavePermission;

    if (serverPermissions.testFlag(Qn::ReadPermission))
        return Qn::ReadPermission;

    return Qn::NoPermissions;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(const QnUserResourcePtr& user, const QnVideoWallResourcePtr& videoWall) const
{
    NX_ASSERT(videoWall);

    if (hasGlobalPermission(user, Qn::GlobalControlVideoWallPermission))
    {
        if (qnCommon->isReadOnly())
            return Qn::ReadPermission;
        return Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;
    }
    return Qn::NoPermissions;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(const QnUserResourcePtr& user, const QnWebPageResourcePtr& webPage) const
{
    NX_ASSERT(webPage);
    Qn::Permissions result = Qn::NoPermissions;

    if (isAccessibleResource(user, webPage) == Access::Forbidden)
        return result;

    result |= Qn::ReadPermission;
    if (qnCommon->isReadOnly())
        return result;

    /* Web Page behaves totally like camera. */
    if (hasGlobalPermission(user, Qn::GlobalEditCamerasPermission))
        result |= Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(const QnUserResourcePtr& user, const QnLayoutResourcePtr& layout) const
{
    TRACE("Calculate permissions of user " << user->getName() << " to layout " << layout->getName())
    NX_ASSERT(layout);
    if (!user || !layout)
        return Qn::NoPermissions;

    //TODO: #GDM Code duplication with QnWorkbenchAccessController::calculatePermissionsInternal
    auto checkReadOnly = [this](Qn::Permissions permissions)
    {
        if (!qnCommon->isReadOnly())
            return permissions;
        return permissions& ~(Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission);
    };

    auto checkLocked = [this, layout](Qn::Permissions permissions)
    {
        if (!layout->locked())
            return permissions;
        return permissions& ~(Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission);
    };

    /* Layouts with desktop cameras are not to be modified but can be removed. */
    if (qnResPool->isAutoGeneratedLayout(layout))
    {
        const auto items = layout->getItems();
        bool hasDesktopCamera = std::any_of(items.cbegin(), items.cend(), [this](const QnLayoutItemData& item)
        {
            QnResourcePtr childResource = qnResPool->getResourceById(item.resource.id);
            return childResource && childResource->hasFlags(Qn::desktop_camera);
        });

        if (hasDesktopCamera)
            return checkReadOnly(Qn::ReadPermission | Qn::RemovePermission);
    }


    /* Calculate base layout permissions */
    auto base = [&]() -> Qn::Permissions
    {
        /* Owner can do anything. */
        if (user->isOwner())
            return Qn::FullLayoutPermissions;

        /* Access to global layouts. Simple check is enough, exported layouts are checked on the client side. */
        if (layout->isShared())
        {
            if (isAccessibleResource(user, layout) == Access::Forbidden)
                return Qn::NoPermissions;

            /* Global layouts editor. */
            if (hasGlobalPermission(user, Qn::GlobalAdminPermission))
                return Qn::FullLayoutPermissions;

            /* Ability to cleanup unused autogenerated layouts. */
            if (hasGlobalPermission(user, Qn::GlobalControlVideoWallPermission)
                && qnResPool->isAutoGeneratedLayout(layout))
                return Qn::FullLayoutPermissions;

            return Qn::ModifyLayoutPermission;
        }

        /* Checking other user's layout*/
        QnUuid ownerId = layout->getParentId();
        if (ownerId != user->getId())
        {
            QnUserResourcePtr owner = qnResPool->getResourceById<QnUserResource>(ownerId);

            if (!owner)
            {
                /* Everybody can modify lite client layout. */
                const auto server = qnResPool->getResourceById<QnMediaServerResource>(ownerId);
                if (server)
                    return Qn::FullLayoutPermissions;

                /* Layout of user, which we don't know of. */
                return hasGlobalPermission(user, Qn::GlobalAdminPermission)
                    ? Qn::FullLayoutPermissions
                    : Qn::NoPermissions;
            }

            /* We can modify layout for user if we can modify this user. */
            Qn::Permissions userPermissions = permissions(user, owner);
            if (userPermissions.testFlag(Qn::SavePermission))
                return Qn::FullLayoutPermissions;

            /* We can see layouts for another users if we are able to see these users. */
            if (userPermissions.testFlag(Qn::ReadPermission))
                return Qn::ModifyLayoutPermission;

            return Qn::NoPermissions;
        }

        /* User can do whatever he wants with own layouts. */
        return Qn::FullLayoutPermissions;
    };

    return checkLocked(checkReadOnly(base()));
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(const QnUserResourcePtr& user, const QnUserResourcePtr& targetUser) const
{
    NX_ASSERT(targetUser);

    auto checkReadOnly = [this](Qn::Permissions permissions)
    {
        if (!qnCommon->isReadOnly())
            return permissions;
        return permissions &~ (Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::WritePasswordPermission | Qn::WriteEmailPermission);
    };

    auto checkUserType = [targetUser](Qn::Permissions permissions)
    {
        switch (targetUser->userType())
        {
        case QnUserType::Ldap:
            return permissions &~ (Qn::WriteNamePermission | Qn::WritePasswordPermission | Qn::WriteEmailPermission);
        case QnUserType::Cloud:
            return permissions &~ (Qn::WritePasswordPermission | Qn::WriteEmailPermission);
        default:
            break;
        }
        return permissions;
    };

    Qn::Permissions result = Qn::NoPermissions;
    if (targetUser == user)
    {
        result |= Qn::ReadPermission | Qn::ReadWriteSavePermission | Qn::WritePasswordPermission; /* Everyone can edit own data. */
    }
    else
    {
        if (hasGlobalPermission(user, Qn::GlobalAdminPermission))
        {
            result |= Qn::ReadPermission;

            /* Admins can only be edited by owner, other users - by all admins. */
            if (user->isOwner() || !hasGlobalPermission(targetUser, Qn::GlobalAdminPermission))
                result |= Qn::ReadWriteSavePermission | Qn::WriteNamePermission | Qn::WritePasswordPermission | Qn::WriteAccessRightsPermission | Qn::RemovePermission;
        }
    }

    return checkReadOnly(checkUserType(result));
}

QnResourceAccessManager::Access QnResourceAccessManager::isAccessibleResource(
    const QnUserResourcePtr& user, const QnResourcePtr& resource) const
{
    NX_ASSERT(resource);

    if (!user || !resource)
        return Access::Forbidden;

    if (!user->isEnabled())
        return Access::Forbidden;

    /* Handling desktop cameras before all other checks. */
    if (resource->hasFlags(Qn::desktop_camera))
        return isAccessibleViaVideowall(user, resource)
        ? Access::ViaVideowall
        : Access::Forbidden;

    QSet<QnUuid> accessible = accessibleResources(user);
    if (accessible.contains(resource->getId()))
        return Access::Directly;

    /* Web Pages behave totally like cameras. */
    bool isMediaResource = resource.dynamicCast<QnVirtualCameraResource>()
        || resource.dynamicCast<QnWebPageResource>();

    bool isLayout = resource->hasFlags(Qn::layout);
    NX_ASSERT(isMediaResource || isLayout);

    auto requiredPermission = isMediaResource
        ? Qn::GlobalAccessAllMediaPermission
        : Qn::GlobalAdminPermission;

    if (hasGlobalPermission(user, requiredPermission))
        return Access::Directly;

    /* Here we are checking if the camera exists on one of the shared layouts, available to given user. */
    if (isMediaResource)
        return isAccessibleViaLayouts(accessible, resource, true)
        ? Access::ViaLayout
        : Access::Forbidden;

    /* Check if layout belongs to videowall. */
    if (isLayout)
        return isAccessibleViaVideowall(user, resource)
        ? Access::ViaVideowall
        : Access::Forbidden;

    return Access::Forbidden;
}

bool QnResourceAccessManager::isAccessibleViaVideowall(const QnUserResourcePtr& user, const QnResourcePtr& resource) const
{
    NX_ASSERT(resource);

    if (!user || !resource)
        return false;

    /* Desktop cameras available only by videowall control permission. */
    if (!hasGlobalPermission(user, Qn::GlobalControlVideoWallPermission))
        return false;

    auto videowalls = qnResPool->getResources<QnVideoWallResource>();

    /* Forbid access if there are no videowalls in the system. */
    if (videowalls.isEmpty())
        return false;

    bool isLayout = resource->hasFlags(Qn::layout);
    bool isCamera = resource->hasFlags(Qn::desktop_camera);

    /* Check if it is our desktop camera */
    if (isCamera && (resource->getName() == user->getName()))
        return true;

    /* Check if camera is placed to videowall. */
    QSet<QnUuid> layoutIds;
    for (const auto& videowall : videowalls)
    {
        for (const auto& item : videowall->items()->getItems())
        {
            if (item.layout.isNull())
                continue;

            layoutIds << item.layout;
        }
    }
    if (isLayout)
        return layoutIds.contains(resource->getId());

    if (isCamera)
        return isAccessibleViaLayouts(layoutIds, resource, false);

    NX_ASSERT(false);
    return false;
}

bool QnResourceAccessManager::isAccessibleViaLayouts(const QSet<QnUuid>& layoutIds, const QnResourcePtr& resource, bool sharedOnly) const
{
    NX_ASSERT(resource);
    if (!resource)
        return false;

    QnUuid resourceId = resource->getId();
    QnLayoutResourceList layouts = qnResPool->getResources(layoutIds).filtered<QnLayoutResource>();
    for (const QnLayoutResourcePtr& layout : layouts)
    {
        /* When checking existing videowall, layouts may be not shared. */
        if (sharedOnly && !layout->isShared())
            continue;

        for (const auto& item : layout->getItems())
        {
            if (item.resource.id == resourceId)
                return true;
        }
    }

    return false;
}

void QnResourceAccessManager::beginUpdateCache()
{
    ++m_cacheUpdateCounter;
    TRACE("beginUpdateCache");
}

void QnResourceAccessManager::endUpdateCache()
{
    int counter = --m_cacheUpdateCounter;
    TRACE("endUpdateCache " << counter);
    NX_ASSERT(counter >= 0);
    if (counter > 0)
        return;

    QSet<QnUuid> invalidatedResources;
    {
        QnMutexLocker lk(&m_mutex);
        if (m_invalidatedResources.isEmpty())
            return;

        invalidatedResources = m_invalidatedResources;
        m_invalidatedResources.clear();
    }

    emit permissionsInvalidated(invalidatedResources);
}

bool QnResourceAccessManager::canCreateStorage(const QnUserResourcePtr& user, const QnUuid& storageParentId) const
{
    if (!user || qnCommon->isReadOnly())
        return false;

    auto server = qnResPool->getResourceById<QnMediaServerResource>(storageParentId);
    return hasPermission(user, server, Qn::SavePermission);
}

bool QnResourceAccessManager::canCreateLayout(const QnUserResourcePtr& user, const QnUuid& layoutParentId) const
{
    if (!user || qnCommon->isReadOnly())
        return false;

    /* Everybody can create own layouts. */
    if (layoutParentId == user->getId())
        return true;

    /* Only admins and videowall-admins can create global layouts. */
    if (layoutParentId.isNull())
    {
        return hasGlobalPermission(user, Qn::GlobalAdminPermission)
            || hasGlobalPermission(user, Qn::GlobalControlVideoWallPermission);
    }

    const auto ownerResource = qnResPool->getResourceById(layoutParentId);

    /* Everybody can create layout for lite client. */
    const auto parentServer = ownerResource.dynamicCast<QnMediaServerResource>();
    if (parentServer)
    {
        if (!parentServer->getServerFlags().testFlag(Qn::SF_HasLiteClient))
            return false;

        /* There can only be one layout for each lite client. */
        const auto existingLayouts = qnResPool->getResources<QnLayoutResource>().filtered(
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
    return hasPermission(user, owner, Qn::SavePermission);
}

bool QnResourceAccessManager::canCreateUser(const QnUserResourcePtr& user, Qn::GlobalPermissions targetPermissions, bool isOwner) const
{
    if (!user || qnCommon->isReadOnly())
        return false;

    /* Nobody can create owners. */
    if (isOwner)
        return false;

    /* Only owner can create admins. */
    if (targetPermissions.testFlag(Qn::GlobalAdminPermission))
        return user->isOwner();

    /* Admins can create other users. */
    return hasGlobalPermission(user, Qn::GlobalAdminPermission);
}

bool QnResourceAccessManager::canCreateVideoWall(const QnUserResourcePtr& user) const
{
    if (!user || qnCommon->isReadOnly())
        return false;

    /* Only admins can create new videowalls (and attach new screens). */
    return hasGlobalPermission(user, Qn::GlobalAdminPermission);
}

bool QnResourceAccessManager::canCreateWebPage(const QnUserResourcePtr& user) const
{
    if (!user || qnCommon->isReadOnly())
        return false;

    /* Only admins can add new web pages. */
    return hasGlobalPermission(user, Qn::GlobalAdminPermission);
}

bool QnResourceAccessManager::canModifyResource(const QnUserResourcePtr& user, const QnResourcePtr& target, const ec2::ApiStorageData& update) const
{
    NX_ASSERT(target.dynamicCast<QnStorageResource>());

    /* Storages cannot be moved from one server to another. */
    if (target->getParentId() != update.parentId)
        return false;

    /* Otherwise - default behavior. */
    return hasPermission(user, target, Qn::SavePermission);
}

bool QnResourceAccessManager::canModifyResource(const QnUserResourcePtr& user, const QnResourcePtr& target, const ec2::ApiLayoutData& update) const
{
    NX_ASSERT(target.dynamicCast<QnLayoutResource>());

    /* If we are changing layout parent, it is equal to creating new layout. */
    if (target->getParentId() != update.parentId)
        return canCreateLayout(user, update.parentId);

    /* Otherwise - default behavior. */
    return hasPermission(user, target, Qn::SavePermission);
}

bool QnResourceAccessManager::canModifyResource(const QnUserResourcePtr& user, const QnResourcePtr& target, const ec2::ApiUserData& update) const
{
    auto userResource = target.dynamicCast<QnUserResource>();
    NX_ASSERT(userResource);

    if (!user || !target)
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

    if (userResource->getDigest() != update.digest)       //TODO: #rvasilenko describe somewhere password changing logic, it looks like hell to me
        requiredPermissions |= Qn::WritePasswordPermission;

    if (userResource->getRawPermissions() != update.permissions)
        requiredPermissions |= Qn::WriteAccessRightsPermission;

    if (userResource->getEmail() != update.email)
        requiredPermissions |= Qn::WriteEmailPermission;

    if (userResource->fullName() != update.fullName)
        requiredPermissions |= Qn::WriteFullNamePermission;

    return hasPermission(user, target, requiredPermissions);
}

bool QnResourceAccessManager::canModifyResource(const QnUserResourcePtr& user, const QnResourcePtr& target, const ec2::ApiVideowallData& update) const
{
    if (!user || qnCommon->isReadOnly())
        return false;

    auto videoWallResource = target.dynamicCast<QnVideoWallResource>();
    NX_ASSERT(videoWallResource);

    auto hasItemsChange = [items = videoWallResource->items()->getItems(), update]
    {
        /* Quick check */
        if (items.size() != update.items.size())
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
        return hasGlobalPermission(user, Qn::GlobalAdminPermission);

    /* Otherwise - default behavior. */
    return hasPermission(user, target, Qn::SavePermission);
}

const QList<Qn::UserRole>& QnResourceAccessManager::predefinedRoles()
{
    static const QList<Qn::UserRole> predefinedRoleList({
        Qn::UserRole::Owner,
        Qn::UserRole::Administrator,
        Qn::UserRole::AdvancedViewer,
        Qn::UserRole::Viewer,
        Qn::UserRole::LiveViewer });

    return predefinedRoleList;
}

QString QnResourceAccessManager::userRoleName(Qn::UserRole userRole)
{
    switch (userRole)
    {
        case Qn::UserRole::Owner:
            return tr("Owner");

        case Qn::UserRole::Administrator:
            return tr("Administrator");

        case Qn::UserRole::AdvancedViewer:
            return tr("Advanced Viewer");

        case Qn::UserRole::Viewer:
            return tr("Viewer");

        case Qn::UserRole::LiveViewer:
            return tr("Live Viewer");

        case Qn::UserRole::CustomUserGroup:
            return tr("Custom Role");

        case Qn::UserRole::CustomPermissions:
            return tr("Custom");
    }

    return QString();
}

QString QnResourceAccessManager::userRoleDescription(Qn::UserRole userRole)
{
    switch (userRole)
    {
        case Qn::UserRole::Owner:
            return tr("Has access to whole system and can do everything.");

        case Qn::UserRole::Administrator:
            return tr("Has access to whole system and can manage it. Can create users.");

        case Qn::UserRole::AdvancedViewer:
            return tr("Can manage all cameras and bookmarks.");

        case Qn::UserRole::Viewer:
            return tr("Can view all cameras and export video.");

        case Qn::UserRole::LiveViewer:
            return tr("Can view live video from all cameras.");

        case Qn::UserRole::CustomUserGroup:
            return tr("Custom user role.");

        case Qn::UserRole::CustomPermissions:
            return tr("Custom permissions.");
    }

    return QString();
}

Qn::GlobalPermissions QnResourceAccessManager::userRolePermissions(Qn::UserRole userRole)
{
    switch (userRole)
    {
        case Qn::UserRole::Owner:
        case Qn::UserRole::Administrator:
            return Qn::GlobalAdminPermissionSet;

        case Qn::UserRole::AdvancedViewer:
            return Qn::GlobalAdvancedViewerPermissionSet;

        case Qn::UserRole::Viewer:
            return Qn::GlobalViewerPermissionSet;

        case Qn::UserRole::LiveViewer:
            return Qn::GlobalLiveViewerPermissionSet;

        case Qn::UserRole::CustomUserGroup:
        case Qn::UserRole::CustomPermissions:
            return Qn::NoGlobalPermissions;
    }

    return Qn::NoGlobalPermissions;
}

Qn::UserRole QnResourceAccessManager::userRole(const QnUserResourcePtr& user) const
{
    if (!user || !user->resourcePool())
        return Qn::UserRole::CustomPermissions;

    if (user->isOwner())
        return Qn::UserRole::Owner;

    QnUuid groupId = user->userGroup();
    if (!groupId.isNull())
        return Qn::UserRole::CustomUserGroup;

    auto permissions = globalPermissions(user);

    if (permissions.testFlag(Qn::GlobalAdminPermission))
        return Qn::UserRole::Administrator;

    switch (permissions)
    {
        case Qn::GlobalAdvancedViewerPermissionSet:
            return Qn::UserRole::AdvancedViewer;

        case Qn::GlobalViewerPermissionSet:
            return Qn::UserRole::Viewer;

        case Qn::GlobalLiveViewerPermissionSet:
            return Qn::UserRole::LiveViewer;

        default:
            return Qn::UserRole::CustomPermissions;
    };
}

QString QnResourceAccessManager::userRoleName(const QnUserResourcePtr& user) const
{
    Qn::UserRole roleType = userRole(user);
    if (roleType == Qn::UserRole::CustomUserGroup)
        return m_userGroups[user->userGroup()].name;

    return userRoleName(roleType);
}

ec2::ApiPredefinedRoleDataList QnResourceAccessManager::getPredefinedRoles()
{
    static ec2::ApiPredefinedRoleDataList kPredefinedRoles;
    if (kPredefinedRoles.empty())
    {
        for (auto role : predefinedRoles())
        {
            kPredefinedRoles.emplace_back(
                userRoleName(role),
                userRolePermissions(role),
                role == Qn::UserRole::Owner);
        }
    }

    return kPredefinedRoles;
}

