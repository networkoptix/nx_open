#include "permissions_resource_access_provider.h"

#include <core/resource_management/resource_pool.h>

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>

#include <core/resource_access/resource_access_manager.h>

QnPermissionsResourceAccessProvider::QnPermissionsResourceAccessProvider(QObject* parent):
    base_type(parent)
{
    connect(qnResPool, &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            for (const auto& user: qnResPool->getResources<QnUserResource>())
                emit accessChanged(user, resource, hasAccess(user, resource));

            if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
            {
                for (const auto& resource: qnResPool->getResources())
                    emit accessChanged(user, resource, hasAccess(user, resource));
            }
        });

    connect(qnResPool, &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            for (const auto& user : qnResPool->getResources<QnUserResource>())
                emit accessChanged(user, resource, hasAccess(user, resource));

            if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
            {
                for (const auto& resource : qnResPool->getResources())
                    emit accessChanged(user, resource, false);
            }
        });
}

QnPermissionsResourceAccessProvider::~QnPermissionsResourceAccessProvider()
{
}

bool QnPermissionsResourceAccessProvider::hasAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    if (!resource || !subject.isValid() || !resource->resourcePool())
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
