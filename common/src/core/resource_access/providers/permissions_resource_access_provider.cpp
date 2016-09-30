#include "permissions_resource_access_provider.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>


QnPermissionsResourceAccessProvider::QnPermissionsResourceAccessProvider(QObject* parent):
    base_type(parent)
{
    connect(qnGlobalPermissionsManager, &QnGlobalPermissionsManager::globalPermissionsChanged,
        this, &QnPermissionsResourceAccessProvider::updateAccessBySubject);
}

QnPermissionsResourceAccessProvider::~QnPermissionsResourceAccessProvider()
{
}

bool QnPermissionsResourceAccessProvider::hasAccessToDesktopCamera(
    const QnResourceAccessSubject& subject, const QnResourcePtr& resource) const
{
    /* Desktop camera can be accessible directly when its name is equal to user's AND
     * if the user has the ability to push his screen. */
    return subject.user()
        && subject.user()->getName() == resource->getName()
        && qnGlobalPermissionsManager->hasGlobalPermission(subject,
            Qn::GlobalControlVideoWallPermission);
}

QnAbstractResourceAccessProvider::Source QnPermissionsResourceAccessProvider::baseSource() const
{
    return Source::permissions;
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

    auto requiredPermission = Qn::GlobalAdminPermission;
    if (isMediaResource(resource))
        requiredPermission = Qn::GlobalAccessAllMediaPermission;
    else if (resource->hasFlags(Qn::videowall))
        requiredPermission = Qn::GlobalControlVideoWallPermission;

    return qnGlobalPermissionsManager->hasGlobalPermission(subject, requiredPermission);
}
