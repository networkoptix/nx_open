#include "permissions_resource_access_provider.h"

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/webpage_resource.h>

#include <core/resource_access/resource_access_manager.h>

QnPermissionsResourceAccessProvider::QnPermissionsResourceAccessProvider(QObject* parent):
    base_type(parent)
{
}

QnPermissionsResourceAccessProvider::~QnPermissionsResourceAccessProvider()
{
}

bool QnPermissionsResourceAccessProvider::hasAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    if (!resource || !subject.isValid())
        return false;

    if (resource->hasFlags(Qn::desktop_camera))
        return hasAccessToDesktopCamera(subject, resource);

    /* Web Pages behave totally like cameras. */
    bool isMediaResource = resource.dynamicCast<QnVirtualCameraResource>()
        || resource.dynamicCast<QnWebPageResource>();

    auto requiredPermission = isMediaResource
        ? Qn::GlobalAccessAllMediaPermission
        : Qn::GlobalAdminPermission;

    return qnResourceAccessManager->hasGlobalPermission(subject, requiredPermission);
}

bool QnPermissionsResourceAccessProvider::hasAccessToDesktopCamera(
    const QnResourceAccessSubject& subject, const QnResourcePtr& resource) const
{
    return subject.user()
        && subject.user()->getName() == resource->getName()
        && qnResourceAccessManager->hasGlobalPermission(subject,
            Qn::GlobalControlVideoWallPermission);
}
