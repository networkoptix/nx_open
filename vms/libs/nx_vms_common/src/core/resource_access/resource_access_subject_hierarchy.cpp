// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_access_subject_hierarchy.h"

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/utils/log/assert.h>

namespace nx::core::access {

class ResourceAccessSubjectHierarchy::Private: public QObject
{
    ResourceAccessSubjectHierarchy* const q;

public:
    Private(ResourceAccessSubjectHierarchy* q,
        QnResourcePool* resourcePool,
        QnUserRolesManager* userGroupsManager)
        :
        q(q),
        resourcePool(resourcePool),
        userGroupsManager(userGroupsManager)
    {
    }

    void handleResourceAdded(const QnResourcePtr& resource)
    {
        const auto user = resource.objectCast<QnUserResource>();
        if (!user)
            return;

        const auto updateUser =
            [this](const QnUserResourcePtr& user)
            {
                const auto groupIds = user->userRoleIds();
                q->addOrUpdate(user->getId(), {groupIds.cbegin(), groupIds.cend()});
            };

        connect(user.get(), &QnUserResource::userRolesChanged, this, updateUser);
        updateUser(user);
    }

    void handleResourceRemoved(const QnResourcePtr& resource)
    {
        const auto user = resource.objectCast<QnUserResource>();
        if (!user)
            return;

        user->disconnect(this);
        q->remove(user->getId());
    }

    void handleGroupAddedOrUpdated(const nx::vms::api::UserRoleData& group)
    {
        q->addOrUpdate(group.getId(), {group.parentRoleIds.cbegin(), group.parentRoleIds.cend()});
    }

    void handleGroupRemoved(const nx::vms::api::UserRoleData& group)
    {
        q->remove(group.getId());
    }

public:
    const QPointer<QnResourcePool> resourcePool;
    const QPointer<QnUserRolesManager> userGroupsManager;
};

ResourceAccessSubjectHierarchy::ResourceAccessSubjectHierarchy(
    QnResourcePool* resourcePool,
    QnUserRolesManager* userGroupsManager,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, resourcePool, userGroupsManager))
{
    if (!NX_ASSERT(resourcePool && userGroupsManager))
        return;

    connect(resourcePool, &QnResourcePool::resourceAdded,
        d.get(), &Private::handleResourceAdded);

    connect(resourcePool, &QnResourcePool::resourceRemoved,
        d.get(), &Private::handleResourceRemoved);

    connect(userGroupsManager, &QnUserRolesManager::userRoleAddedOrUpdated,
        d.get(), &Private::handleGroupAddedOrUpdated);

    connect(userGroupsManager, &QnUserRolesManager::userRoleRemoved,
        d.get(), &Private::handleGroupRemoved);

    for (const auto& group: userGroupsManager->userRoles())
        d->handleGroupAddedOrUpdated(group);

    for (const auto& user: resourcePool->getResources<QnUserResource>())
        d->handleResourceAdded(user);
}

ResourceAccessSubjectHierarchy::~ResourceAccessSubjectHierarchy()
{
    // Required here for forward-declared scoped pointer destruction.
}

QnResourceAccessSubject ResourceAccessSubjectHierarchy::subjectById(const QnUuid& id) const
{
    if (!NX_ASSERT(d->resourcePool && d->userGroupsManager))
        return {};

    if (const auto& user = d->resourcePool->getResourceById<QnUserResource>(id))
        return {user};

    return {d->userGroupsManager->userRole(id)};
}

} // namespace nx::core::access
