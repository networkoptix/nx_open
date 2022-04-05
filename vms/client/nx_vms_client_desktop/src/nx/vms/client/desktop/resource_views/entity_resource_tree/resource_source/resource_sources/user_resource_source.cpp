// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_resource_source.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

UserResourceSource::UserResourceSource(
    const QnResourcePool* resourcePool,
    const QnUserResourcePtr& currentUser,
    const QnUuid& roleId)
    :
    base_type(),
    m_resourcePool(resourcePool),
    m_currentUser(currentUser),
    m_roleId(roleId)
{
    if (!NX_ASSERT(m_resourcePool))
        return;

    connect(m_resourcePool, &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            if (!resource->hasFlags(Qn::user) || resource->hasFlags(Qn::fake))
                return;

            const auto user = resource.staticCast<QnUserResource>();
            if (user == m_currentUser)
                return;

            connect(user.get(), &QnUserResource::userRolesChanged,
                this, &UserResourceSource::onUserRolesChanged);

            connect(user.get(), &QnUserResource::enabledChanged,
                this, &UserResourceSource::onEnabledChanged);

            if (user->firstRoleId() == m_roleId && user->isEnabled())
                emit resourceAdded(resource);
        });

    connect(m_resourcePool, &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            if (resource->hasFlags(Qn::user))
            {
                resource->disconnect(this);
                if (resource.staticCast<QnUserResource>()->firstRoleId() == m_roleId)
                    emit resourceRemoved(resource);
            }
        });
}

void UserResourceSource::onUserRolesChanged(const QnUserResourcePtr& user,
    const std::vector<QnUuid>& previousRoleIds)
{
    if ((previousRoleIds.empty() ? QnUuid() : previousRoleIds.front()) == m_roleId)
        emit resourceRemoved(user);

    if (user->firstRoleId() == m_roleId)
        emit resourceAdded(user);
}

void UserResourceSource::onEnabledChanged(const QnUserResourcePtr& user)
{
    if (user->firstRoleId() != m_roleId)
        return;

    if (user->isEnabled())
        emit resourceAdded(user);
    else
        emit resourceRemoved(user);
}

QVector<QnResourcePtr> UserResourceSource::getResources()
{
    QVector<QnResourcePtr> result;

    const auto userResources = m_resourcePool->getResourcesWithFlag(Qn::user);
    for (const auto& userResource: userResources)
    {
        if (userResource->hasFlags(Qn::fake))
           continue;

        const auto user = userResource.staticCast<QnUserResource>();

        if (user == m_currentUser)
            continue;

        connect(user.get(), &QnUserResource::userRolesChanged,
            this, &UserResourceSource::onUserRolesChanged);

        connect(user.get(), &QnUserResource::enabledChanged,
            this, &UserResourceSource::onEnabledChanged);

        if (user->firstRoleId() == m_roleId && user->isEnabled())
            result.push_back(userResource);
    }

    return result;
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
