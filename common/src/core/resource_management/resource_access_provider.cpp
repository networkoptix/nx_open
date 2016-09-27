#include "resource_access_provider.h"

#include <common/common_globals.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>

QnResourceAccessProvider::QnResourceAccessProvider()
{
}

bool QnResourceAccessProvider::isAccessibleResource(const QnResourceAccessSubject& subject, const QnResourcePtr& resource)
{
    return resourceAccess(subject, resource) != Access::Forbidden;
}

QnResourceAccessProvider::Access QnResourceAccessProvider::resourceAccess(const QnResourceAccessSubject& subject, const QnResourcePtr& resource)
{
    if (!subject.isValid() || !resource)
        return Access::Forbidden;

    /* Handling desktop cameras before all other checks. */
    if (resource->hasFlags(Qn::desktop_camera))
        return isAccessibleViaVideowall(subject, resource)
        ? Access::ViaVideowall
        : Access::Forbidden;

    QSet<QnUuid> accessible = sharedResources(subject);
    if (accessible.contains(resource->getId()))
        return Access::Directly;

    /* Web Pages behave totally like cameras. */
    bool isMediaResource = resource.dynamicCast<QnVirtualCameraResource>()
        || resource.dynamicCast<QnWebPageResource>();

    bool isLayout = resource->hasFlags(Qn::layout);
    NX_ASSERT(isMediaResource || isLayout);

    auto requiredPermission = isMediaResource
        ? Qn::GlobalAccessAllMediaPermission
        : Qn::GlobalAdminPermission;

    if (qnResourceAccessManager->hasGlobalPermission(subject, requiredPermission))
        return Access::Directly;

    /* Here we are checking if the camera exists on one of the shared layouts, available to given user. */
    if (isMediaResource)
        return isAccessibleViaLayouts(accessible, resource, true)
        ? Access::ViaLayout
        : Access::Forbidden;

    /* Check if layout belongs to videowall. */
    if (isLayout)
        return isAccessibleViaVideowall(subject, resource)
        ? Access::ViaVideowall
        : Access::Forbidden;

    return Access::Forbidden;
}

QnIndirectAccessProviders QnResourceAccessProvider::indirectlyAccessibleLayouts(const QnResourceAccessSubject& subject)
{
    if (!qnResourceAccessManager->hasGlobalPermission(subject, Qn::GlobalControlVideoWallPermission))
        return QnIndirectAccessProviders();

    QnIndirectAccessProviders indirectlyAccessible;
    for (const auto& videowall : qnResPool->getResources<QnVideoWallResource>())
    {
        for (const auto& item : videowall->items()->getItems())
        {
            if (item.layout.isNull())
                continue;

            indirectlyAccessible[item.layout] << videowall;
        }
    }

    return indirectlyAccessible;
}

QSet<QnUuid> QnResourceAccessProvider::sharedResources(const QnResourceAccessSubject& subject)
{

    return qnResourceAccessManager->accessibleResources(subject);
}

bool QnResourceAccessProvider::isAccessibleViaVideowall(const QnResourceAccessSubject& subject, const QnResourcePtr& resource)
{
    NX_ASSERT(resource);

    if (!resource)
        return false;

    /* Desktop cameras available only by videowall control permission. */
    if (!qnResourceAccessManager->hasGlobalPermission(subject, Qn::GlobalControlVideoWallPermission))
        return false;

    auto videowalls = qnResPool->getResources<QnVideoWallResource>();

    /* Forbid access if there are no videowalls in the system. */
    if (videowalls.isEmpty())
        return false;

    bool isLayout = resource->hasFlags(Qn::layout);
    bool isDesktopCamera = resource->hasFlags(Qn::desktop_camera);

    /* Check if it is our desktop camera */
    if (isDesktopCamera
        && subject.user()
        && (resource->getName() == subject.user()->getName()))
        return true;

    /* Check if camera is placed to videowall. */
    QSet<QnUuid> layoutIds;
    for (const auto& videowall : videowalls)
    {
        for (const auto& item : videowall->items()->getItems())
        {
            if (item.layout.isNull())
                continue;

            layoutIds << item.layout;
        }
    }
    if (isLayout)
        return layoutIds.contains(resource->getId());

    if (isDesktopCamera)
        return isAccessibleViaLayouts(layoutIds, resource, false);

    NX_ASSERT(false);
    return false;
}

bool QnResourceAccessProvider::isAccessibleViaLayouts(const QSet<QnUuid>& layoutIds, const QnResourcePtr& resource, bool sharedOnly)
{
    NX_ASSERT(resource);
    if (!resource)
        return false;

    QnUuid resourceId = resource->getId();
    QnLayoutResourceList layouts = qnResPool->getResources<QnLayoutResource>(layoutIds);
    for (const QnLayoutResourcePtr& layout : layouts)
    {
        /* When checking existing videowall, layouts may be not shared. */
        if (sharedOnly && !layout->isShared())
            continue;

        for (const auto& item : layout->getItems())
        {
            if (item.resource.id == resourceId)
                return true;
        }
    }

    return false;
}
