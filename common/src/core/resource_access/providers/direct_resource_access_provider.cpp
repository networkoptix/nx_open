#include "direct_resource_access_provider.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>


QnDirectResourceAccessProvider::QnDirectResourceAccessProvider(QObject* parent):
    base_type(parent)
{
    connect(qnResPool, &QnResourcePool::resourceAdded, this,
        &QnDirectResourceAccessProvider::handleResourceAdded);
    connect(qnResPool, &QnResourcePool::resourceRemoved, this,
        &QnDirectResourceAccessProvider::handleResourceRemoved);

    connect(qnUserRolesManager, &QnUserRolesManager::userRoleAddedOrUpdated, this,
        &QnDirectResourceAccessProvider::handleRoleAddedOrUpdated);
    connect(qnUserRolesManager, &QnUserRolesManager::userRoleRemoved, this,
        &QnDirectResourceAccessProvider::handleRoleRemoved);
}

QnDirectResourceAccessProvider::~QnDirectResourceAccessProvider()
{
}

bool QnDirectResourceAccessProvider::hasAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    if (!acceptable(subject, resource))
        return false;

    NX_ASSERT(m_accessibleResources.contains(subject.id()));
    return m_accessibleResources[subject.id()].contains(resource->getId());
}

bool QnDirectResourceAccessProvider::acceptable(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    return resource && resource->resourcePool() && subject.isValid();
}

bool QnDirectResourceAccessProvider::calculateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    NX_ASSERT(acceptable(subject, resource));
    if (!acceptable(subject, resource))
        return false;

    return true;
}

void QnDirectResourceAccessProvider::updateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource)
{
    NX_ASSERT(acceptable(subject, resource));
    if (!acceptable(subject, resource))
        return;

    auto& accessible = m_accessibleResources[subject.id()];
    auto targetId = resource->getId();

    bool oldValue = accessible.contains(targetId);
    bool newValue = calculateAccess(subject, resource);
    if (oldValue == newValue)
        return;

    if (newValue)
        accessible.insert(targetId);
    else
        accessible.remove(targetId);

    emit accessChanged(subject, resource, newValue);
}

void QnDirectResourceAccessProvider::handleResourceAdded(const QnResourcePtr& resource)
{
    for (const auto& user : qnResPool->getResources<QnUserResource>())
        updateAccess(user, resource);

    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
    {
        for (const auto& resource : qnResPool->getResources())
        {
            /* We have already update access to ourselves before */
            if (user != resource)
                updateAccess(user, resource);
        }

        connect(user, &QnUserResource::permissionsChanged, this,
            [this, user]
            {
                for (const auto& resource : qnResPool->getResources())
                    updateAccess(user, resource);
            });
    }
}

void QnDirectResourceAccessProvider::handleResourceRemoved(const QnResourcePtr& resource)
{
    auto resourceId = resource->getId();

    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
    {
        disconnect(user, nullptr, this, nullptr);

        QnResourceAccessSubject subject(user);

        NX_ASSERT(m_accessibleResources.contains(resourceId));
        auto& accessible = m_accessibleResources[resourceId];

        for (const auto& targetResource : qnResPool->getResources(accessible.values()))
        {
            QnUuid targetId = targetResource->getId();
            accessible.remove(targetId);
            emit accessChanged(subject, targetResource, false);
        }

        /* We should get only own user resource there. */
        NX_ASSERT(accessible.contains(resourceId));
        emit accessChanged(subject, resource, false);
        accessible.remove(resourceId);
        NX_ASSERT(accessible.isEmpty());

        m_accessibleResources.remove(resourceId);
    }

    for (const auto& user : qnResPool->getResources<QnUserResource>())
    {
        if (user == resource)
            continue;

        QnResourceAccessSubject subject(user);
        auto& accessible = m_accessibleResources[subject.id()];
        if (!accessible.contains(resourceId))
            continue;

        accessible.remove(resourceId);
        emit accessChanged(subject, resource, false);
    }
}

void QnDirectResourceAccessProvider::handleRoleAddedOrUpdated(
    const ec2::ApiUserGroupData& userRole)
{
    for (const auto& resource : qnResPool->getResources())
    {
        /* We have already update access to ourselves before */
        updateAccess(userRole, resource);
    }
}

void QnDirectResourceAccessProvider::handleRoleRemoved(const ec2::ApiUserGroupData& userRole)
{
    QnResourceAccessSubject subject(userRole);
    auto id = subject.id();

    NX_ASSERT(m_accessibleResources.contains(id));
    auto& accessible = m_accessibleResources[id];

    for (const auto& targetResource : qnResPool->getResources(accessible.values()))
    {
        QnUuid targetId = targetResource->getId();
        accessible.remove(targetId);
        emit accessChanged(subject, targetResource, false);
    }
    NX_ASSERT(accessible.isEmpty());
    m_accessibleResources.remove(id);
}
