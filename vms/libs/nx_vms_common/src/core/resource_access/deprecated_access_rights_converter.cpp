// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "deprecated_access_rights_converter.h"

#include <QtCore/QHash>
#include <QtCore/QPointer>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/data/access_rights_data.h>

namespace nx::core::access {

using namespace nx::vms::api;

class DeprecatedAccessRightsConverter::Private: public QObject
{
    DeprecatedAccessRightsConverter* q;

public:
    explicit Private(DeprecatedAccessRightsConverter* q,
        QnResourcePool* resourcePool,
        QnUserRolesManager* userGroupsManager,
        QnSharedResourcesManager* sharedResourcesManager,
        AccessRightsManager* destinationAccessRightsManager)
        :
        q(q),
        resourcePool(resourcePool),
        userGroupsManager(userGroupsManager),
        sharedResourcesManager(sharedResourcesManager),
        accessRightsManager(destinationAccessRightsManager)
    {
    }

    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);
    void handleGroupAddedOrUpdated(const UserRoleData& group);
    void handleGroupRemoved(const UserRoleData& group);
    void handleSharedResourcesChanged(const QnResourceAccessSubject& subject);

    void updateSubject(const QnUuid& subjectId);

    const QPointer<QnResourcePool> resourcePool;
    const QPointer<QnUserRolesManager> userGroupsManager;
    const QPointer<QnSharedResourcesManager> sharedResourcesManager;
    const QPointer<AccessRightsManager> accessRightsManager;

    mutable nx::Mutex mutex;
    QHash<QnUuid /*subjectId*/, std::map<QnUuid, AccessRights>> sharedResourceRights;
    QHash<QnUuid /*subjectId*/, GlobalPermissions> globalPermissions;
};

// -----------------------------------------------------------------------------------------------
// DeprecatedAccessRightsConverter

DeprecatedAccessRightsConverter::DeprecatedAccessRightsConverter(
    QnResourcePool* resourcePool,
    QnUserRolesManager* userGroupsManager,
    QnSharedResourcesManager* sharedResourcesManager,
    AccessRightsManager* destinationAccessRightsManager,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this,
        resourcePool,
        userGroupsManager,
        sharedResourcesManager,
        destinationAccessRightsManager))
{
    NX_CRITICAL(d->resourcePool
        && d->userGroupsManager
        && d->sharedResourcesManager
        && d->accessRightsManager);

    connect(d->resourcePool, &QnResourcePool::resourceAdded,
        d.get(), &Private::handleResourceAdded, Qt::DirectConnection);

    connect(d->resourcePool, &QnResourcePool::resourceRemoved,
        d.get(), &Private::handleResourceRemoved, Qt::DirectConnection);

    connect(d->userGroupsManager, &QnUserRolesManager::userRoleAddedOrUpdated,
        d.get(), &Private::handleGroupAddedOrUpdated, Qt::DirectConnection);

    connect(d->userGroupsManager, &QnUserRolesManager::userRoleRemoved,
        d.get(), &Private::handleGroupRemoved, Qt::DirectConnection);

    connect(d->sharedResourcesManager, &QnSharedResourcesManager::sharedResourcesChanged,
        d.get(), &Private::handleSharedResourcesChanged, Qt::DirectConnection);

    for (const auto& group: QnPredefinedUserRoles::list())
        d->handleGroupAddedOrUpdated(group);
}

DeprecatedAccessRightsConverter::~DeprecatedAccessRightsConverter()
{
    // Required here for forward-declared scoped pointer destruction.
}

void DeprecatedAccessRightsConverter::beforeUpdate()
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    d->sharedResourceRights.clear();
    d->globalPermissions.clear();
}

void DeprecatedAccessRightsConverter::afterUpdate()
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    for (const auto& subjectId: nx::utils::keyRange(d->globalPermissions))
        d->updateSubject(subjectId);
}

// -----------------------------------------------------------------------------------------------
// DeprecatedAccessRightsConverter::Private

void DeprecatedAccessRightsConverter::Private::handleResourceAdded(const QnResourcePtr& resource)
{
    const auto user = resource.objectCast<QnUserResource>();
    if (!user)
        return;

    const auto updateUserPermissions =
        [this](const QnUserResourcePtr& user)
        {
            const auto id = user->getId();

            NX_MUTEX_LOCKER lk(&mutex);
            globalPermissions[id] = user->getRawPermissions();

            if (!q->isUpdating())
                updateSubject(id);
        };

    connect(user.get(), &QnUserResource::permissionsChanged,
        this, updateUserPermissions, Qt::DirectConnection);
    updateUserPermissions(user);
}

void DeprecatedAccessRightsConverter::Private::handleResourceRemoved(const QnResourcePtr& resource)
{
    const auto user = resource.objectCast<QnUserResource>();
    if (!user)
        return;

    const auto id = user->getId();

    NX_MUTEX_LOCKER lk(&mutex);
    sharedResourceRights.remove(id);
    globalPermissions.remove(id);
    user->disconnect(this);
}

void DeprecatedAccessRightsConverter::Private::handleGroupAddedOrUpdated(const UserRoleData& group)
{
    NX_MUTEX_LOCKER lk(&mutex);
    globalPermissions[group.id] = group.permissions;

    if (!q->isUpdating())
        updateSubject(group.id);
}

void DeprecatedAccessRightsConverter::Private::handleGroupRemoved(const UserRoleData& group)
{
    NX_MUTEX_LOCKER lk(&mutex);
    sharedResourceRights.remove(group.id);
    globalPermissions.remove(group.id);
}

void DeprecatedAccessRightsConverter::Private::handleSharedResourcesChanged(
    const QnResourceAccessSubject& subject)
{
    if (!NX_ASSERT(sharedResourcesManager))
        return;

    NX_MUTEX_LOCKER lk(&mutex);
    sharedResourceRights[subject.id()] = sharedResourcesManager->directAccessRights(subject);

    if (!q->isUpdating())
        updateSubject(subject.id());
}

void DeprecatedAccessRightsConverter::Private::updateSubject(const QnUuid& subjectId)
{
    if (!globalPermissions.contains(subjectId) || !accessRightsManager)
        return;

    const auto globalPermissions = this->globalPermissions.value(subjectId);
    if (globalPermissions.testFlag(GlobalPermission::admin))
    {
        accessRightsManager->setOwnResourceAccessMap(subjectId,
            {{AccessRightsManager::kAnyResourceId, kAdminAccessRights}});
        return;
    }

    ResourceAccessMap accessMap;
    const auto accessRights = globalPermissionsToAccessRights(globalPermissions).setFlag(
        AccessRight::view, globalPermissions.testFlag(GlobalPermission::accessAllMedia));
    accessMap[AccessRightsManager::kAnyResourceId] = accessRights;

    // For much simpler logic we don't separate access rights flags per resource type here.
    // Not applicable permissions will be filtered out by resource access resolvers.

    for (const auto& [resourceId, sharedRights]: sharedResourceRights.value(subjectId))
        accessMap[resourceId] = accessRights | sharedRights;

    accessRightsManager->setOwnResourceAccessMap(subjectId, accessMap);
}

} // namespace nx::core::access
