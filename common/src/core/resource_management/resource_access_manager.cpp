#include "resource_access_manager.h"

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>


QnResourceAccessManager::QnResourceAccessManager(QObject* parent /*= nullptr*/) :
    base_type(parent)
{
    connect(qnCommon, &QnCommonModule::readOnlyChanged, this, [this]() { clearCache();  });

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
    });

    connect(qnResPool, &QnResourcePool::resourceRemoved, this, [this, removeResourceFromCache](const QnResourcePtr& resource)
    {
        disconnect(resource, nullptr, this, nullptr);
        removeResourceFromCache(resource);
    });
}

void QnResourceAccessManager::reset(const ec2::ApiAccessRightsDataList& accessRights)
{
    m_accessibleResources.clear();
    for (const auto& item : accessRights)
    {
        QSet<QnUuid> &accessibleResources = m_accessibleResources[item.userId];
        for (const auto& id : item.resourceIds)
            accessibleResources << id;
    }
}

QSet<QnUuid> QnResourceAccessManager::accessibleResources(const QnUuid& userId) const
{
    return m_accessibleResources[userId];
}

void QnResourceAccessManager::setAccessibleResources(const QnUuid& userId, const QSet<QnUuid>& resources)
{
    m_accessibleResources[userId] = resources;
}

Qn::GlobalPermissions QnResourceAccessManager::globalPermissions(const QnUserResourcePtr &user) const
{
    Qn::GlobalPermissions result(Qn::NoGlobalPermissions);

    /* Totally correct behavior, thus we simplify permissions checking on the logged-out client. */
    if (!user)
        return result;

    NX_ASSERT(user->resourcePool(), Q_FUNC_INFO, "Requesting permissions for non-pool user");

    QnUuid userId = user->getId();

    auto iter = m_globalPermissionsCache.find(userId);
    if (iter != m_globalPermissionsCache.cend())
        return *iter;

    result = user->getPermissions();

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

    return result;
}

void QnResourceAccessManager::clearCache(const QnUuid& id)
{
    m_globalPermissionsCache.remove(id);
}

void QnResourceAccessManager::clearCache()
{
    m_globalPermissionsCache.clear();
}
