// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_access_subject_hierarchy.h"

#include <QtCore/QPointer>
#include <QtCore/QSignalBlocker>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/common/user_management/user_group_manager.h>

using namespace nx::vms::common;

namespace nx::core::access {

namespace {

constexpr auto userFilter =
    [](const QnResourcePtr& resource)
    {
        return resource->hasFlags(Qn::ResourceFlag::user);
    };

} // namespace;

class ResourceAccessSubjectHierarchy::Private: public QObject
{
    ResourceAccessSubjectHierarchy* const q;

public:
    Private(ResourceAccessSubjectHierarchy* q,
        QnResourcePool* resourcePool,
        UserGroupManager* userGroupManager)
        :
        q(q),
        resourcePool(resourcePool),
        userGroupManager(userGroupManager)
    {
    }

    void handleResourcesAdded(const QnResourceList& resources)
    {
        const auto updateUser =
            [this](const QnUserResourcePtr& user)
            {
                q->addOrUpdate(user->getId(), nx::utils::toQSet(user->groupIds()));
            };

        for (const auto& resource: resources)
        {
            const auto user = resource.objectCast<QnUserResource>();
            if (!user)
                continue;

            connect(user.get(), &QnUserResource::userGroupsChanged,
                this, updateUser, Qt::DirectConnection);

            updateUser(user);
        }
    }

    void handleResourcesRemoved(const QnResourceList& resources)
    {
        for (const auto& resource: resources)
        {
            if (!resource->hasFlags(Qn::ResourceFlag::user))
                continue;

            resource->disconnect(this);
            q->remove(resource->getId());
        }
    }

    void handleGroupAddedOrUpdated(const nx::vms::api::UserGroupData& group)
    {
        q->addOrUpdate(group.getId(), {group.parentGroupIds.cbegin(), group.parentGroupIds.cend()});
    }

    void handleGroupRemoved(const nx::vms::api::UserGroupData& group)
    {
        q->remove(group.getId());
    }

    void handleGroupsReset()
    {
        {
            const QSignalBlocker lk(q);

            q->clear();

            for (const auto& group: userGroupManager->groups())
                handleGroupAddedOrUpdated(group);

            handleResourcesAdded(resourcePool->getResources(userFilter));
        }

        emit q->reset();
    }

public:
    const QPointer<QnResourcePool> resourcePool;
    const QPointer<UserGroupManager> userGroupManager;
};

ResourceAccessSubjectHierarchy::ResourceAccessSubjectHierarchy(
    QnResourcePool* resourcePool,
    UserGroupManager* userGroupManager,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, resourcePool, userGroupManager))
{
    if (!NX_ASSERT(resourcePool && userGroupManager))
        return;

    connect(resourcePool, &QnResourcePool::resourcesAdded,
        d.get(), &Private::handleResourcesAdded, Qt::DirectConnection);

    connect(resourcePool, &QnResourcePool::resourcesRemoved,
        d.get(), &Private::handleResourcesRemoved, Qt::DirectConnection);

    connect(userGroupManager, &UserGroupManager::addedOrUpdated,
        d.get(), &Private::handleGroupAddedOrUpdated, Qt::DirectConnection);

    connect(userGroupManager, &UserGroupManager::removed,
        d.get(), &Private::handleGroupRemoved, Qt::DirectConnection);

    connect(userGroupManager, &UserGroupManager::reset,
        d.get(), &Private::handleGroupsReset, Qt::DirectConnection);

    for (const auto& group: userGroupManager->groups())
        d->handleGroupAddedOrUpdated(group);

    d->handleResourcesAdded(resourcePool->getResources(userFilter));
}

ResourceAccessSubjectHierarchy::~ResourceAccessSubjectHierarchy()
{
    // Required here for forward-declared scoped pointer destruction.
}

QnResourceAccessSubject ResourceAccessSubjectHierarchy::subjectById(const nx::Uuid& id) const
{
    if (!NX_ASSERT(d->resourcePool && d->userGroupManager))
        return {};

    if (const auto& user = d->resourcePool->getResourceById<QnUserResource>(id))
        return {user};

    return {d->userGroupManager->find(id).value_or(nx::vms::api::UserGroupData{})};
}

QnUserResourceList ResourceAccessSubjectHierarchy::usersInGroups(const QSet<nx::Uuid>& groupIds) const
{
    if (!NX_ASSERT(d->resourcePool))
        return {};

    return d->resourcePool->getResourcesByIds<QnUserResource>(recursiveMembers(groupIds));
}

} // namespace nx::core::access
