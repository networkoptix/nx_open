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


QnResourceAccessManager::QnResourceAccessManager(QObject* parent /*= nullptr*/) :
    base_type(parent),
    m_mutex(QnMutex::NonRecursive)
{
    /* This change affects all accessible resources. */
    connect(qnCommon,& QnCommonModule::readOnlyChanged, this, [this](bool readOnly)
    {
        Q_UNUSED(readOnly);
        QnMutexLocker lk(&m_mutex);
        m_permissionsCache.clear();
    });

    connect(qnResPool,& QnResourcePool::resourceAdded, this, [this](const QnResourcePtr& resource)
    {
        if (const QnLayoutResourcePtr& layout = resource.dynamicCast<QnLayoutResource>())
        {
            connect(layout,& QnResource::parentIdChanged,           this,& QnResourceAccessManager::invalidateResourceCache); /* To make layouts global */
            connect(layout,& QnLayoutResource::userCanEditChanged,  this,& QnResourceAccessManager::invalidateResourceCache);
            connect(layout,& QnLayoutResource::lockedChanged,       this,& QnResourceAccessManager::invalidateResourceCache);
        }

        if (const QnUserResourcePtr& user = resource.dynamicCast<QnUserResource>())
        {
            connect(user,& QnUserResource::permissionsChanged,  this,& QnResourceAccessManager::invalidateResourceCache);
            connect(user,& QnUserResource::userGroupChanged,    this,& QnResourceAccessManager::invalidateResourceCache);
        }
    });

    connect(qnResPool,& QnResourcePool::resourceRemoved, this, [this](const QnResourcePtr& resource)
    {
        disconnect(resource, nullptr, this, nullptr);
    });
    connect(qnResPool,& QnResourcePool::resourceRemoved, this,& QnResourceAccessManager::invalidateResourceCache);
}

void QnResourceAccessManager::resetAccessibleResources(const ec2::ApiAccessRightsDataList& accessibleResourcesList)
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
    QnMutexLocker lk(&m_mutex);
    m_permissionsCache.clear();
    m_globalPermissionsCache.clear();
    m_userGroups.clear();
    for (const auto& group : userGroups)
        m_userGroups.insert(group.id, group);
}

ec2::ApiUserGroupData QnResourceAccessManager::userGroup(const QnUuid& groupId) const
{
    QnMutexLocker lk(&m_mutex);
    return m_userGroups.value(groupId);
}

void QnResourceAccessManager::addOrUpdateUserGroup(const ec2::ApiUserGroupData& userGroup)
{
    QnMutexLocker lk(&m_mutex);
    m_userGroups[userGroup.id] = userGroup;
}

void QnResourceAccessManager::removeUserGroup(const QnUuid& groupId)
{
    QnMutexLocker lk(&m_mutex);
    m_userGroups.remove(groupId);
}

QSet<QnUuid> QnResourceAccessManager::accessibleResources(const QnUuid& userId) const
{
    QnMutexLocker lk(&m_mutex);
    return m_accessibleResources[userId];
}

void QnResourceAccessManager::setAccessibleResources(const QnUuid& userId, const QSet<QnUuid>& resources)
{
    QnMutexLocker lk(&m_mutex);
    m_accessibleResources[userId] = resources;

    for (auto iter = m_permissionsCache.begin(); iter != m_permissionsCache.end();)
    {
        if (iter.key().userId == userId)
            iter = m_permissionsCache.erase(iter);
        else
            ++iter;
    }
}

Qn::GlobalPermissions QnResourceAccessManager::globalPermissions(const QnUserResourcePtr& user) const
{
    NX_ASSERT(user, Q_FUNC_INFO, "We must not request permissions for absent user.");
    if (!user)
        return Qn::NoGlobalPermissions;

    /* Handle just-created user situation. */
    if (user->flags().testFlag(Qn::local))
        return user->getRawPermissions();

    NX_ASSERT(user->resourcePool(), Q_FUNC_INFO, "Requesting permissions for non-pool user");

    QnUuid userId = user->getId();

    {
        QnMutexLocker lk(&m_mutex);
        auto iter = m_globalPermissionsCache.find(userId);
        if (iter != m_globalPermissionsCache.cend())
            return *iter;
    }

    Qn::GlobalPermissions result = user->getRawPermissions();
    QnUuid groupId = user->userGroup();

    if (user->isOwner() || result.testFlag(Qn::GlobalAdminPermission))
    {
        result |= Qn::GlobalAdminPermissionsSet;
    }
    else if (!groupId.isNull())
    {
        result |= userGroup(groupId).permissions;   /*< If the group does not exist, permissions will be empty. */
    }

    {
        QnMutexLocker lk(&m_mutex);
        m_globalPermissionsCache.insert(userId, result);
    }

    return result;
}

bool QnResourceAccessManager::hasGlobalPermission(const QnUserResourcePtr& user, Qn::GlobalPermission requiredPermission) const
{
    return globalPermissions(user).testFlag(requiredPermission);
}

Qn::Permissions QnResourceAccessManager::permissions(const QnUserResourcePtr& user, const QnResourcePtr& resource) const
{
    NX_ASSERT(user && resource, Q_FUNC_INFO, "We must not request permissions for absent resources.");
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

bool QnResourceAccessManager::hasPermission(const QnUserResourcePtr& user, const QnResourcePtr& resource, Qn::Permission requiredPermission) const
{
    return permissions(user, resource).testFlag(requiredPermission);
}

bool QnResourceAccessManager::canCreateResource(const QnUserResourcePtr& user, const QnResourcePtr& target) const
{
    NX_ASSERT(target);

    if (qnCommon->isReadOnly())
        return false;

    /* Check new layouts creating. */
    if (QnLayoutResourcePtr layout = target.dynamicCast<QnLayoutResource>())
        return canCreateLayoutInternal(user, layout->getParentId());

    /* Check new users creating. */
    if (QnUserResourcePtr targetUser = target.dynamicCast<QnUserResource>())
        return canCreateUserInternal(user, targetUser->getRawPermissions(), targetUser->isOwner());

    if (QnStorageResourcePtr storage = target.dynamicCast<QnStorageResource>())
        return canCreateStorageInternal(user, storage->getParentId());

    if (QnVideoWallResourcePtr videoWall = target.dynamicCast<QnVideoWallResource>())
        return canCreateVideoWallInternal(user);

    if (QnWebPageResourcePtr webPage = target.dynamicCast<QnWebPageResource>())
        return canCreateWebPageInternal(user);

    /* Other resources cannot be added manually. */
    return false;
}

bool QnResourceAccessManager::canCreateResource(const QnUuid& userId, const ec2::ApiStorageData& data) const
{
    return canCreateStorageInternal(qnResPool->getResourceById<QnUserResource>(userId), data.parentId);
}

bool QnResourceAccessManager::canCreateResource(const QnUuid& userId, const ec2::ApiLayoutData& data) const
{
    return canCreateLayoutInternal(qnResPool->getResourceById<QnUserResource>(userId), data.parentId);
}

bool QnResourceAccessManager::canCreateResource(const QnUuid& userId, const ec2::ApiUserData& data) const
{
    return canCreateUserInternal(qnResPool->getResourceById<QnUserResource>(userId), data.permissions, data.isAdmin);
}

bool QnResourceAccessManager::canCreateResource(const QnUuid& userId, const ec2::ApiVideowallData& data) const
{
    Q_UNUSED(data);
    return canCreateVideoWallInternal(qnResPool->getResourceById<QnUserResource>(userId));
}

bool QnResourceAccessManager::canCreateResource(const QnUuid& userId, const ec2::ApiWebPageData& data) const
{
    Q_UNUSED(data);
    return canCreateWebPageInternal(qnResPool->getResourceById<QnUserResource>(userId));
}

void QnResourceAccessManager::invalidateResourceCache(const QnResourcePtr& resource)
{
    NX_ASSERT(resource);
    if (!resource)
        return;

    QnUuid id = resource->getId();

    QnMutexLocker lk(&m_mutex);
    m_globalPermissionsCache.remove(id);
    for (auto iter = m_permissionsCache.begin(); iter != m_permissionsCache.end();)
    {
        if (iter.key().userId == id || iter.key().resourceId == id)
            iter = m_permissionsCache.erase(iter);
        else
            ++iter;
    }
}

Qn::Permissions QnResourceAccessManager::calculatePermissions(const QnUserResourcePtr& user, const QnResourcePtr& target) const
{
    NX_ASSERT(target);

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
    NX_ASSERT(camera);

    if (!isAccessibleResource(user, camera))
        return Qn::NoPermissions;

    Qn::Permissions result = Qn::ReadPermission;
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

    if (!isAccessibleResource(user, server))
        return Qn::NoPermissions;

    if (hasGlobalPermission(user, Qn::GlobalEditServersPermissions))
    {
        if (qnCommon->isReadOnly())
            return Qn::ReadPermission;
        return Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;
    }
    return Qn::ReadPermission;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(const QnUserResourcePtr& user, const QnStorageResourcePtr& storage) const
{
    NX_ASSERT(storage);
    auto server = storage->getParentServer();
    NX_ASSERT(server);

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

    if (!isAccessibleResource(user, videoWall))
        return Qn::NoPermissions;

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

    if (!isAccessibleResource(user, webPage))
        return Qn::NoPermissions;

    Qn::Permissions result = Qn::ReadPermission;
    if (qnCommon->isReadOnly())
        return result;

    /* Web Page behaves totally like camera. */
    if (hasGlobalPermission(user, Qn::GlobalEditCamerasPermission))
        return Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(const QnUserResourcePtr& user, const QnLayoutResourcePtr& layout) const
{
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


    if (qnResPool->isAutoGeneratedLayout(layout))
    {
        const auto items = layout->getItems();
        bool hasDesktopCamera = std::any_of(items.cbegin(), items.cend(), [this](const QnLayoutItemData& item)
        {
            QnResourcePtr childResource = qnResPool->getResourceById(item.resource.id);
            return childResource && childResource->hasFlags(Qn::desktop_camera);
        });

        /* Layouts with desktop cameras are not to be modified. */
        if (hasDesktopCamera)
            return checkReadOnly(Qn::ReadPermission | Qn::RemovePermission);
    }


    /* Calculate base layout permissions */
    auto base = [&]() -> Qn::Permissions
    {
        /* Owner can do anything. */
        if (user->isOwner())
            return Qn::FullLayoutPermissions;

        QnUuid ownerId = layout->getParentId();

        /* Access to global layouts. */
        if (ownerId.isNull())
        {
            if (!isAccessibleResource(user, layout))
                return Qn::NoPermissions;

            /* Global layouts editor. */
            if (hasGlobalPermission(user, Qn::GlobalEditLayoutsPermission))
                return Qn::FullLayoutPermissions;

            return Qn::ModifyLayoutPermission;
        }

        /* Checking other user's layout*/
        if (layout->getParentId() != user->getId())
        {
            /* Nobody's layout. Bug. */
            QnUserResourcePtr owner = qnResPool->getResourceById<QnUserResource>(ownerId);
            NX_ASSERT(owner);
            if (!owner)
                return hasGlobalPermission(user, Qn::GlobalAdminPermission)
                    ? Qn::FullLayoutPermissions
                    : Qn::NoPermissions;

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
        if (layout->userCanEdit())
            return Qn::FullLayoutPermissions;

        /* Can structurally modify but cannot save. */
        return Qn::ModifyLayoutPermission;
    };

    return checkLocked(checkReadOnly(base()));
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(const QnUserResourcePtr& user, const QnUserResourcePtr& targetUser) const
{
    NX_ASSERT(targetUser);

    Qn::Permissions result = Qn::NoPermissions;
    if (targetUser == user)
    {
        if (qnCommon->isReadOnly())
            return result | Qn::ReadPermission;

        result |= Qn::ReadWriteSavePermission | Qn::WritePasswordPermission; /* Everyone can edit own data. */
    }

    if ((targetUser != user) && hasGlobalPermission(user, Qn::GlobalAdminPermission))
    {
        result |= Qn::ReadPermission;
        if (qnCommon->isReadOnly())
            return result;

        /* Admins can only be edited by owner, other users - by all admins. */
        if (user->isOwner() || !hasGlobalPermission(targetUser, Qn::GlobalAdminPermission))
            result |= Qn::ReadWriteSavePermission | Qn::WriteNamePermission | Qn::WritePasswordPermission | Qn::WriteAccessRightsPermission | Qn::RemovePermission;
    }

    /* Nobody can edit LDAP-provided parameters. */
    if (targetUser->isLdap())
    {
        result &= ~Qn::WriteNamePermission;
        result &= ~Qn::WritePasswordPermission;
        result &= ~Qn::WriteEmailPermission;
    }

    return result;
}

bool QnResourceAccessManager::isAccessibleResource(const QnUserResourcePtr& user, const QnResourcePtr& resource) const
{
    NX_ASSERT(resource);

    if (!user || !resource)
        return false;

    {
        QnMutexLocker lk(&m_mutex);
        if (m_accessibleResources[user->getId()].contains(resource->getId()))
            return true;
    }

    auto requiredPermission = [this, resource]()
    {
        if (resource.dynamicCast<QnLayoutResource>())
            return Qn::GlobalAccessAllLayoutsPermission;

        if (resource.dynamicCast<QnVirtualCameraResource>())
            return Qn::GlobalAccessAllCamerasPermission;

        if (resource.dynamicCast<QnMediaServerResource>())
            return Qn::GlobalAccessAllServersPermission;

        if (resource.dynamicCast<QnVideoWallResource>())
            return Qn::GlobalControlVideoWallPermission;

        /* Web Pages behave totally like cameras. */
        if (resource.dynamicCast<QnWebPageResource>())
            return Qn::GlobalAccessAllCamerasPermission;

        /* Default value (e.g. for users). */
        return Qn::GlobalAdminPermission;
    };

    return hasGlobalPermission(user, requiredPermission());
}

bool QnResourceAccessManager::canCreateStorageInternal(const QnUserResourcePtr& user, const QnUuid& storageParentId) const
{
    if (!user || qnCommon->isReadOnly())
        return false;
    auto server = qnResPool->getResourceById<QnMediaServerResource>(storageParentId);
    return hasPermission(user, server, Qn::SavePermission);
}

bool QnResourceAccessManager::canCreateLayoutInternal(const QnUserResourcePtr& user, const QnUuid& layoutParentId) const
{
    /* Everybody can create own layouts. */
    if (layoutParentId == user->getId())
        return true;

    /* Somebody can create global layouts. */
    if (layoutParentId.isNull())
        return hasGlobalPermission(user, Qn::GlobalEditLayoutsPermission);

    QnUserResourcePtr owner = qnResPool->getResourceById<QnUserResource>(layoutParentId);
    if (owner)
        return false;

    /* We can create layout for user if we can modify this user. */
    return hasPermission(user, owner, Qn::SavePermission);
}

bool QnResourceAccessManager::canCreateUserInternal(const QnUserResourcePtr& user, Qn::GlobalPermissions targetPermissions, bool isOwner) const
{
    /* Nobody can create owners. */
    if (isOwner)
        return false;

    /* Only owner can create admins. */
    if (targetPermissions.testFlag(Qn::GlobalAdminPermission))
        return user->isOwner();

    /* Admins can create other users. */
    return hasGlobalPermission(user, Qn::GlobalAdminPermission);
}

bool QnResourceAccessManager::canCreateVideoWallInternal(const QnUserResourcePtr& user) const
{
    /* Only admins can create new videowalls (and attach new screens). */
    return hasGlobalPermission(user, Qn::GlobalAdminPermission);
}

bool QnResourceAccessManager::canCreateWebPageInternal(const QnUserResourcePtr& user) const
{
    /* Only admins can add new web pages. */
    return hasGlobalPermission(user, Qn::GlobalAdminPermission);
}
