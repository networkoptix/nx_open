// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "global_permissions_watcher.h"

#include <QtCore/QPointer>
#include <QtCore/QSignalBlocker>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/common/user_management/user_group_manager.h>

namespace nx::core::access {

using namespace nx::vms::api;
using namespace nx::vms::common;

// ------------------------------------------------------------------------------------------------
// GlobalPermissionsWatcher::Private

class GlobalPermissionsWatcher::Private: public QObject
{
    GlobalPermissionsWatcher* const q;

    const QPointer<QnResourcePool> resourcePool;
    const QPointer<UserGroupManager> userGroupManager;

public:
    Private(
        GlobalPermissionsWatcher* q,
        QnResourcePool* resourcePool,
        UserGroupManager* userGroupManager)
        :
        q(q),
        resourcePool(resourcePool),
        userGroupManager(userGroupManager)
    {
        if (!NX_ASSERT(resourcePool && userGroupManager))
            return;

        connect(resourcePool, &QnResourcePool::resourcesAdded,
            this, &Private::handleResourcesAdded, Qt::DirectConnection);

        connect(resourcePool, &QnResourcePool::resourcesRemoved,
            this, &Private::handleResourcesRemoved, Qt::DirectConnection);

        connect(userGroupManager, &UserGroupManager::addedOrUpdated,
            this, &Private::handleGroupAddedOrUpdated, Qt::DirectConnection);

        connect(userGroupManager, &UserGroupManager::removed,
            this, &Private::handleGroupRemoved, Qt::DirectConnection);

        connect(userGroupManager, &UserGroupManager::reset,
            this, &Private::handleGroupsReset, Qt::DirectConnection);

        for (const auto& user: resourcePool->getResources<QnUserResource>())
            handleUserChanged(user);

        for (const auto& group: userGroupManager->groups())
            handleGroupAddedOrUpdated(group);
    }

    void handleResourcesAdded(const QnResourceList& resources)
    {
        for (const auto& resource: resources)
        {

            const auto user = resource.objectCast<QnUserResource>();
            if (!user)
                continue;

            handleUserChanged(user);

            if (user->isAdministrator())
                continue;

            connect(user.get(), &QnUserResource::permissionsChanged,
                this, &Private::handleUserChanged, Qt::DirectConnection);
        }
    }

    void handleResourcesRemoved(const QnResourceList& resources)
    {
        for (const auto& resource: resources)
        {
            if (!resource->hasFlags(Qn::ResourceFlag::user))
                continue;

            resource->disconnect(this);
            handleSubjectRemoved(resource->getId());
        }
    }

    void handleUserChanged(const QnUserResourcePtr& user)
    {
        if (!NX_ASSERT(user))
            return;

        static constexpr GlobalPermissions kAdministratorPermissions{
            GlobalPermission::administrator | GlobalPermission::powerUser | GlobalPermission::viewLogs};

        const auto permissions = user->isAdministrator()
            ? kAdministratorPermissions
            : user->getRawPermissions();

        handleSubjectChanged(user->getId(), permissions);
    }

    void handleGroupAddedOrUpdated(const UserGroupData& group)
    {
        handleSubjectChanged(group.id, group.permissions);
    }

    void handleGroupRemoved(const UserGroupData& group)
    {
        handleSubjectRemoved(group.id);
    }

    void handleGroupsReset()
    {
        if (q->isUpdating())
            return;
        {
            const QSignalBlocker blocker(q);

            NX_MUTEX_LOCKER lk(&mutex);
            globalPermissions.clear();
            lk.unlock();

            for (const auto& user: resourcePool->getResources<QnUserResource>())
                handleUserChanged(user);

            for (const auto& group: userGroupManager->groups())
                handleGroupAddedOrUpdated(group);
        }

        emit q->globalPermissionsReset();
    }

    void handleSubjectChanged(const nx::Uuid& subjectId, GlobalPermissions permissions)
    {
        NX_MUTEX_LOCKER lk(&mutex);
        if (permissions == globalPermissions.value(subjectId))
            return;

        globalPermissions[subjectId] = permissions;

        if (q->isUpdating())
            return;

        lk.unlock();
        emit q->ownGlobalPermissionsChanged(subjectId);
    }

    void handleSubjectRemoved(const nx::Uuid& subjectId)
    {
        NX_MUTEX_LOCKER lk(&mutex);
        if (!globalPermissions.remove(subjectId) || q->isUpdating())
            return;

        lk.unlock();
        emit q->ownGlobalPermissionsChanged(subjectId);
    }

public:
    QHash<nx::Uuid, GlobalPermissions> globalPermissions;
    mutable nx::Mutex mutex;
};

// ------------------------------------------------------------------------------------------------
// GlobalPermissionsWatcher

GlobalPermissionsWatcher::GlobalPermissionsWatcher(
    QnResourcePool* resourcePool,
    UserGroupManager* userGroupManager,
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

GlobalPermissions GlobalPermissionsWatcher::ownGlobalPermissions(const nx::Uuid& subjectId) const
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
