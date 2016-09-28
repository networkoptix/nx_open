#include "permissions_resource_access_provider.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>


QnPermissionsResourceAccessProvider::QnPermissionsResourceAccessProvider(QObject* parent):
    base_type(parent)
{
    connect(qnResPool, &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            for (const auto& user: qnResPool->getResources<QnUserResource>())
                updateAccess(user, resource);

            if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
            {
                for (const auto& resource: qnResPool->getResources())
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
        });

    connect(qnResPool, &QnResourcePool::resourceRemoved, this,
        &QnPermissionsResourceAccessProvider::cleanAccess);

    connect(qnUserRolesManager, &QnUserRolesManager::userRoleAddedOrUpdated, this,
        &QnPermissionsResourceAccessProvider::handleRoleAddedOrUpdated);
}

QnPermissionsResourceAccessProvider::~QnPermissionsResourceAccessProvider()
{
}

bool QnPermissionsResourceAccessProvider::hasAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    if (!acceptable(subject, resource))
        return false;

    NX_ASSERT(m_accessibleResources.contains(subject.id()));
    return m_accessibleResources[subject.id()].contains(resource->getId());
}

bool QnPermissionsResourceAccessProvider::hasAccessToDesktopCamera(
    const QnResourceAccessSubject& subject, const QnResourcePtr& resource) const
{
    /* Desktop camera can be accessible directly when its name is equal to user's AND
     * if the user has the ability to push his screen. */
    return subject.user()
        && subject.user()->getName() == resource->getName()
        && qnResourceAccessManager->hasGlobalPermission(subject,
            Qn::GlobalControlVideoWallPermission);
}

bool QnPermissionsResourceAccessProvider::acceptable(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    return resource && resource->resourcePool() && subject.isValid();
}

bool QnPermissionsResourceAccessProvider::calculateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    NX_ASSERT(acceptable(subject, resource));
    if (!acceptable(subject, resource))
        return false;

    if (resource == subject.user())
        return true;

    if (resource->hasFlags(Qn::desktop_camera))
        return hasAccessToDesktopCamera(subject, resource);

    /* Web Pages behave totally like cameras. */
    bool isMediaResource = resource.dynamicCast<QnVirtualCameraResource>()
        || resource->hasFlags(Qn::web_page);

    bool isVideoWall = resource->hasFlags(Qn::videowall);

    auto requiredPermission = Qn::GlobalAdminPermission;
    if (isMediaResource)
        requiredPermission = Qn::GlobalAccessAllMediaPermission;
    else if (isVideoWall)
        requiredPermission = Qn::GlobalControlVideoWallPermission;

    return qnResourceAccessManager->hasGlobalPermission(subject, requiredPermission);
}

void QnPermissionsResourceAccessProvider::updateAccess(const QnResourceAccessSubject& subject,
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

void QnPermissionsResourceAccessProvider::cleanAccess(const QnResourcePtr& resource)
{
    auto resourceId = resource->getId();

    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
    {
        disconnect(user, nullptr, this, nullptr);

        QnResourceAccessSubject subject(user);
        auto id = subject.id();

        NX_ASSERT(m_accessibleResources.contains(id));
        auto& accessible = m_accessibleResources[id];

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

        m_accessibleResources.remove(id);
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

void QnPermissionsResourceAccessProvider::handleRoleAddedOrUpdated(
    const ec2::ApiUserGroupData& userRole)
{
    for (const auto& resource : qnResPool->getResources())
    {
        /* We have already update access to ourselves before */
        updateAccess(userRole, resource);
    }
}
