// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "global_permissions_watcher.h"

#include <QtCore/QPointer>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/thread/mutex.h>

namespace nx::core::access {

using namespace nx::vms::api;

namespace {

GlobalPermissions filterDependentPermissions(GlobalPermissions source)
{
    GlobalPermissions result = source;
    if (!result.testFlag(GlobalPermission::viewArchive))
        result &= ~(GlobalPermission::viewBookmarks | GlobalPermission::exportArchive);

    if (!result.testFlag(GlobalPermission::viewBookmarks))
        result &= ~GlobalPermissions(GlobalPermission::manageBookmarks);

    return result;
}

} // namespace

// ------------------------------------------------------------------------------------------------
// GlobalPermissionsWatcher::Private

class GlobalPermissionsWatcher::Private: public QObject
{
    GlobalPermissionsWatcher* const q;

public:
    Private(GlobalPermissionsWatcher* q,
        QnResourcePool* resourcePool,
        QnUserRolesManager* userGroupManager)
        :
        q(q)
    {
        if (!NX_ASSERT(resourcePool && userGroupManager))
            return;

        connect(resourcePool, &QnResourcePool::resourceAdded,
            this, &Private::handleResourceAdded, Qt::DirectConnection);

        connect(resourcePool, &QnResourcePool::resourceRemoved,
            this, &Private::handleResourceRemoved, Qt::DirectConnection);

        connect(userGroupManager, &QnUserRolesManager::userRoleAddedOrUpdated,
            this, &Private::handleGroupAddedOrUpdated, Qt::DirectConnection);

        connect(userGroupManager, &QnUserRolesManager::userRoleRemoved,
            this, &Private::handleGroupRemoved, Qt::DirectConnection);

        for (const auto& user: resourcePool->getResources<QnUserResource>())
            handleUserChanged(user);

        for (const auto& group: userGroupManager->userRoles())
            handleGroupAddedOrUpdated(group);
    }

    void handleResourceAdded(const QnResourcePtr& resource)
    {
        const auto user = resource.objectCast<QnUserResource>();
        if (!user)
            return;

        handleUserChanged(user);

        if (user->isOwner())
            return;

        connect(user.get(), &QnUserResource::permissionsChanged,
            this, &Private::handleUserChanged, Qt::DirectConnection);
    }

    void handleResourceRemoved(const QnResourcePtr& resource)
    {
        const auto user = resource.objectCast<QnUserResource>();
        if (!user)
            return;

        user->disconnect(this);
        handleSubjectRemoved(user->getId());
    }

    void handleUserChanged(const QnUserResourcePtr& user)
    {
        if (!NX_ASSERT(user))
            return;

        static constexpr GlobalPermissions kOwnerPermissions{
            GlobalPermission::admin | GlobalPermission::viewLogs};

        const auto permissions = user->isOwner()
            ? kOwnerPermissions
            : user->getRawPermissions();

        handleSubjectChanged(user->getId(), permissions);
    }

    void handleGroupAddedOrUpdated(const UserRoleData& group)
    {
        handleSubjectChanged(group.id, group.permissions);
    }

    void handleGroupRemoved(const UserRoleData& group)
    {
        handleSubjectRemoved(group.id);
    }

    void handleSubjectChanged(const QnUuid& subjectId, GlobalPermissions permissions)
    {
        permissions = filterDependentPermissions(permissions);

        NX_MUTEX_LOCKER lk(&mutex);
        if (permissions == globalPermissions.value(subjectId))
            return;

        globalPermissions[subjectId] = permissions;

        if (q->isUpdating())
            return;

        lk.unlock();
        emit q->ownGlobalPermissionsChanged(subjectId);
    }

    void handleSubjectRemoved(const QnUuid& subjectId)
    {
        NX_MUTEX_LOCKER lk(&mutex);
        if (!globalPermissions.remove(subjectId) || q->isUpdating())
            return;

        lk.unlock();
        emit q->ownGlobalPermissionsChanged(subjectId);
    }

public:
    QHash<QnUuid, GlobalPermissions> globalPermissions;
    mutable nx::Mutex mutex;
};

// ------------------------------------------------------------------------------------------------
// GlobalPermissionsWatcher

GlobalPermissionsWatcher::GlobalPermissionsWatcher(
    QnResourcePool* resourcePool,
    QnUserRolesManager* userGroupManager,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, resourcePool, userGroupManager))
{
}

GlobalPermissionsWatcher::~GlobalPermissionsWatcher()
{
    // Required here for forward-declared scoped pointer destruction.
}

GlobalPermissions GlobalPermissionsWatcher::ownGlobalPermissions(const QnUuid& subjectId) const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->globalPermissions.value(subjectId);
}

void GlobalPermissionsWatcher::beforeUpdate()
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    d->globalPermissions.clear();
}

void GlobalPermissionsWatcher::afterUpdate()
{
    emit globalPermissionsReset();
}

} // namespace nx::core::access
