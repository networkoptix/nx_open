#include "resource_access_manager.h"

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/webpage_resource.h>

#include <nx/utils/log/assert.h>


QnResourceAccessManager::QnResourceAccessManager(QObject* parent /*= nullptr*/) :
    base_type(parent),
    m_readOnlyMode(qnCommon->isReadOnly())
{
    connect(qnCommon, &QnCommonModule::readOnlyChanged, this, [this](bool readOnly)
    {
        m_readOnlyMode = readOnly;
        clearCache();
    });

    auto removeResourceFromCache = [this](const QnResourcePtr& resource)
    {
        if (resource)
            clearCache(resource->getId());
    };

    connect(qnResPool, &QnResourcePool::resourceAdded, this, [this, removeResourceFromCache](const QnResourcePtr& resource)
    {
        if (const QnLayoutResourcePtr &layout = resource.dynamicCast<QnLayoutResource>())
        {
            connect(layout, &QnResource::parentIdChanged,           this, removeResourceFromCache); /* To make layouts global */
            connect(layout, &QnLayoutResource::userCanEditChanged,  this, removeResourceFromCache);
            connect(layout, &QnLayoutResource::lockedChanged,       this, removeResourceFromCache);
        }

        if (const QnUserResourcePtr& user = resource.dynamicCast<QnUserResource>())
        {
            connect(user, &QnUserResource::permissionsChanged,  this, removeResourceFromCache);
            connect(user, &QnUserResource::userGroupChanged,    this, removeResourceFromCache);
        }

    });

    connect(qnResPool, &QnResourcePool::resourceRemoved, this, [this, removeResourceFromCache](const QnResourcePtr& resource)
    {
        disconnect(resource, nullptr, this, nullptr);
        removeResourceFromCache(resource);
    });
}

void QnResourceAccessManager::resetAccessibleResources(const ec2::ApiAccessRightsDataList& accessRights)
{
    m_accessibleResources.clear();
    for (const auto& item : accessRights)
    {
        QSet<QnUuid> &accessibleResources = m_accessibleResources[item.userId];
        for (const auto& id : item.resourceIds)
            accessibleResources << id;
    }
    m_permissionsCache.clear();
}

void QnResourceAccessManager::resetUserGroups(const ec2::ApiUserGroupDataList& userGroups)
{
    m_userGroups = userGroups;
}

QSet<QnUuid> QnResourceAccessManager::accessibleResources(const QnUuid& userId) const
{
    return m_accessibleResources[userId];
}

void QnResourceAccessManager::setAccessibleResources(const QnUuid& userId, const QSet<QnUuid>& resources)
{
    m_accessibleResources[userId] = resources;

    for (auto iter = m_permissionsCache.begin(); iter != m_permissionsCache.end();)
    {
        if (iter.key().userId == userId)
            iter = m_permissionsCache.erase(iter);
        else
            ++iter;
    }
}

ec2::ApiUserGroupDataList QnResourceAccessManager::userGroups() const
{
    return m_userGroups;
}

Qn::GlobalPermissions QnResourceAccessManager::globalPermissions(const QnUserResourcePtr &user) const
{
    NX_ASSERT(user, Q_FUNC_INFO, "We must not request permissions for absent user.");
    if (!user)
        return Qn::NoGlobalPermissions;

    /* Handle just-created user situation. */
    if (user->flags().testFlag(Qn::local))
        return user->getRawPermissions();

    NX_ASSERT(user->resourcePool(), Q_FUNC_INFO, "Requesting permissions for non-pool user");

    QnUuid userId = user->getId();

    auto iter = m_globalPermissionsCache.find(userId);
    if (iter != m_globalPermissionsCache.cend())
        return *iter;

    Qn::GlobalPermissions result = user->getRawPermissions();

    if (user->isOwner() || result.testFlag(Qn::GlobalOwnerPermission))
        result |= Qn::GlobalOwnerPermissionsSet;

    if (result.testFlag(Qn::GlobalAdminPermission))
        result |= Qn::GlobalAdminPermissionsSet;

    result = undeprecate(result);
    m_globalPermissionsCache.insert(userId, result);

    return result;
}

bool QnResourceAccessManager::hasGlobalPermission(const QnUserResourcePtr &user, Qn::GlobalPermission requiredPermission) const
{
    return globalPermissions(user).testFlag(requiredPermission);
}

Qn::Permissions QnResourceAccessManager::permissions(const QnUserResourcePtr& user, const QnResourcePtr& resource) const
{
    NX_ASSERT(user && resource, Q_FUNC_INFO, "We must not request permissions for absent resources.");
    if (!user || !resource)
        return Qn::NoPermissions;

    NX_ASSERT(resource->resourcePool(), Q_FUNC_INFO, "Requesting permissions for non-pool resource");

    PermissionKey key(user->getId(), resource->getId());
    auto iter = m_permissionsCache.find(key);
    if (iter != m_permissionsCache.cend())
        return *iter;

    Qn::Permissions result = calculatePermissions(user, resource);
    m_permissionsCache.insert(key, result);
    return result;
}

bool QnResourceAccessManager::hasPermission(const QnUserResourcePtr& user, const QnResourcePtr& resource, Qn::Permission requiredPermission) const
{
    return permissions(user, resource).testFlag(requiredPermission);
}

Qn::GlobalPermissions QnResourceAccessManager::undeprecate(Qn::GlobalPermissions permissions)
{
    Qn::GlobalPermissions result = permissions;

    if (result.testFlag(Qn::DeprecatedEditCamerasPermission))
    {
        result &= ~Qn::DeprecatedEditCamerasPermission;
        result |= Qn::GlobalEditCamerasPermission | Qn::GlobalPtzControlPermission;
    }

    if (result.testFlag(Qn::DeprecatedViewExportArchivePermission))
    {
        result &= ~Qn::DeprecatedViewExportArchivePermission;
        result |= Qn::GlobalViewArchivePermission | Qn::GlobalExportPermission;
    }

    if (result.testFlag(Qn::DeprecatedGlobalEditUsersPermission))
        result &= ~Qn::DeprecatedGlobalEditUsersPermission;

    return result;
}

void QnResourceAccessManager::clearCache(const QnUuid& id)
{
    m_globalPermissionsCache.remove(id);
    for (auto iter = m_permissionsCache.begin(); iter != m_permissionsCache.end();)
    {
        if (iter.key().userId == id || iter.key().resourceId == id)
            iter = m_permissionsCache.erase(iter);
        else
            ++iter;
    }
}

void QnResourceAccessManager::clearCache()
{
    m_globalPermissionsCache.clear();
    m_permissionsCache.clear();
}

Qn::Permissions QnResourceAccessManager::calculatePermissions(const QnUserResourcePtr &user, const QnResourcePtr &target) const
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

    if (QnVideoWallResourcePtr videoWall = target.dynamicCast<QnVideoWallResource>())
        return calculatePermissionsInternal(user, videoWall);

    if (QnWebPageResourcePtr webPage = target.dynamicCast<QnWebPageResource>())
        return calculatePermissionsInternal(user, webPage);

    NX_ASSERT("invalid resource type");
    return Qn::NoPermissions;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(const QnUserResourcePtr &user, const QnVirtualCameraResourcePtr &camera) const
{
    NX_ASSERT(camera);

    if (!isAccessibleResource(user, camera))
        return Qn::NoPermissions;

    Qn::Permissions result = Qn::ReadPermission;
    if (hasGlobalPermission(user, Qn::GlobalExportPermission))
        result |= Qn::ExportPermission;

    if (hasGlobalPermission(user, Qn::GlobalPtzControlPermission))
        result |= Qn::WritePtzPermission;

    if (m_readOnlyMode)
        return result;

    if (hasGlobalPermission(user, Qn::GlobalEditCamerasPermission))
        result |= Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(const QnUserResourcePtr &user, const QnMediaServerResourcePtr &server) const
{
    NX_ASSERT(server);

    if (!isAccessibleResource(user, server))
        return Qn::NoPermissions;

    if (hasGlobalPermission(user, Qn::GlobalEditServersPermissions))
    {
        if (m_readOnlyMode)
            return Qn::ReadPermission;
        return Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;
    }
    return Qn::ReadPermission;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(const QnUserResourcePtr &user, const QnVideoWallResourcePtr &videoWall) const
{
    NX_ASSERT(videoWall);

    if (!isAccessibleResource(user, videoWall))
        return Qn::NoPermissions;

    if (hasGlobalPermission(user, Qn::GlobalEditVideoWallPermission))
    {
        if (m_readOnlyMode)
            return Qn::ReadPermission;
        return Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;
    }
    return Qn::NoPermissions;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(const QnUserResourcePtr &user, const QnWebPageResourcePtr &webPage) const
{
    NX_ASSERT(webPage);

    if (!isAccessibleResource(user, webPage))
        return Qn::NoPermissions;

    Qn::Permissions result = Qn::ReadPermission;
    if (m_readOnlyMode)
        return result;

    /* Web Page behaves totally like camera. */
    if (hasGlobalPermission(user, Qn::GlobalEditCamerasPermission))
        return Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(const QnUserResourcePtr &user, const QnLayoutResourcePtr &layout) const
{
    NX_ASSERT(layout);
    if (!user || !layout)
        return Qn::NoPermissions;

    //TODO: #GDM Code duplication with QnWorkbenchAccessController::calculatePermissionsInternal
    auto checkReadOnly = [this](Qn::Permissions permissions)
    {
        if (!m_readOnlyMode)
            return permissions;
        return permissions &~(Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission);
    };

    auto checkLocked = [this, layout](Qn::Permissions permissions)
    {
        if (!layout->locked())
            return permissions;
        return permissions &~(Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission);
    };


    if (qnResPool->isAutoGeneratedLayout(layout))
    {
        const auto items = layout->getItems();
        bool hasDesktopCamera = std::any_of(items.cbegin(), items.cend(), [this](const QnLayoutItemData &item)
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
        /* Admin can do whatever he wants. */
        if (hasGlobalPermission(user, Qn::GlobalEditLayoutsPermission))
            return Qn::FullLayoutPermissions;

        /* Access to global layouts. */
        if (layout->getParentId().isNull())
            return isAccessibleResource(user, layout)
                ? Qn::ReadPermission | Qn::WritePermission | Qn::AddRemoveItemsPermission
                : Qn::NoPermissions;

        /* Viewer cannot view other user's layouts. */
        if (layout->getParentId() != user->getId())
            return Qn::NoPermissions;

        /* User can do whatever he wants with own layouts. */
        if (layout->userCanEdit())
            return Qn::FullLayoutPermissions;

        /* Can structurally modify but cannot save. */
        return Qn::ReadPermission | Qn::WritePermission | Qn::AddRemoveItemsPermission;
    };

    return checkLocked(checkReadOnly(base()));
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(const QnUserResourcePtr &user, const QnUserResourcePtr &targetUser) const
{
    NX_ASSERT(targetUser);

    Qn::Permissions result = Qn::NoPermissions;
    if (targetUser == user)
    {
        if (m_readOnlyMode)
            return result | Qn::ReadPermission;

        result |= Qn::ReadWriteSavePermission | Qn::WritePasswordPermission; /* Everyone can edit own data. */
        result |= Qn::CreateLayoutPermission; /* Everyone can create a layout for themselves */
    }

    if ((targetUser != user) && hasGlobalPermission(user, Qn::GlobalAdminPermission))
    {
        result |= Qn::ReadPermission;
        if (m_readOnlyMode)
            return result;

        /* Layout-admin can create layouts. */ //TODO: #GDM Should we refactor it in 3.0?
        if (hasGlobalPermission(user, Qn::GlobalEditLayoutsPermission))
            result |= Qn::CreateLayoutPermission;

        /* Admins can only be edited by owner, other users - by all admins. */
        if (hasGlobalPermission(user, Qn::GlobalOwnerPermission) || !hasGlobalPermission(targetUser, Qn::GlobalAdminPermission))
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

bool QnResourceAccessManager::isAccessibleResource(const QnUserResourcePtr &user, const QnResourcePtr &resource) const
{
    NX_ASSERT(resource);

    if (!user || !resource)
        return false;

    if (m_accessibleResources[user->getId()].contains(resource->getId()))
        return true;

    auto requiredPermission = [this, resource]()
    {
        if (resource.dynamicCast<QnLayoutResource>())
            return Qn::GlobalAccessAllLayoutsPermission;

        if (resource.dynamicCast<QnVirtualCameraResource>())
            return Qn::GlobalAccessAllCamerasPermission;

        if (resource.dynamicCast<QnMediaServerResource>())
            return Qn::GlobalAccessAllServersPermission;

        if (resource.dynamicCast<QnVideoWallResource>())
            return Qn::GlobalEditVideoWallPermission;

        /* Web Pages behave totally like cameras. */
        if (resource.dynamicCast<QnWebPageResource>())
            return Qn::GlobalAccessAllCamerasPermission;

        /* Default value (e.g. for users). */
        return Qn::GlobalAdminPermission;
    };

    return hasGlobalPermission(user, requiredPermission());
}
