// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_descriptor.h"

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_layouts_manager.h>
#include <nx/vms/client/desktop/cross_system/cross_system_camera_resource.h>
#include <nx/vms/client/desktop/cross_system/cross_system_layout_resource.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>

namespace nx::vms::client::desktop {

namespace {

static const QString kCloudScheme = "cloud://";

/** System Id placeholder for resources, not bound to any System (e.g. cloud layouts). */
static const QString kGenericCloudSystemId = QnUuid().toSimpleString();

QString resourcePath(const QnUuid& resourceId, const QString& cloudSystemId)
{
    if (NX_ASSERT(!cloudSystemId.isEmpty()))
        return nx::format(kCloudScheme + "%1.%2", cloudSystemId, resourceId.toSimpleString());

    return {};
}

QString resourcePath(const QnResourcePtr& resource, bool forceCloud)
{
    if (resource->hasFlags(Qn::exported_layout))
        return resource->getUrl();

    if (resource->hasFlags(Qn::local_media))
        return resource->getUrl();

    if (resource->hasFlags(Qn::web_page))
        return resource->getUrl();

    if (resource.dynamicCast<CrossSystemLayoutResource>())
        return resourcePath(resource->getId(), kGenericCloudSystemId);

    if (const auto camera = resource.dynamicCast<CrossSystemCameraResource>())
        return resourcePath(camera->getId(), camera->systemId());

    const auto systemContext = SystemContext::fromResource(resource);
    const bool belongsToOtherContext = (systemContext != appContext()->currentSystemContext());
    if (NX_ASSERT(systemContext) && (forceCloud || belongsToOtherContext))
    {
        // TODO: #sivanov Update cloudSystemId in the current system context module information
        // when the system is bound to the cloud or vise versa.

        // Using module information is generally better for the remote cloud systems, but we cannot
        // use it for the current system as it may be bound to the cloud in the current session.
        const QString cloudSystemId = belongsToOtherContext
            ? systemContext->moduleInformation().cloudSystemId
            : systemContext->globalSettings()->cloudSystemId();

        if (NX_ASSERT(!cloudSystemId.isEmpty()))
        {
            return resourcePath(resource->getId(), cloudSystemId);
        }
    }

    return {};
}

} // namespace

nx::vms::common::ResourceDescriptor descriptor(const QnResourcePtr& resource, bool forceCloud)
{
    if (!NX_ASSERT(resource))
        return {};

    return {
        .id=resource->getId(),
        .path=resourcePath(resource, forceCloud),
        .name=resource->getName()
    };
}

/** Find Resource in a corresponding System Context. */
QnResourcePtr getResourceByDescriptor(const nx::vms::common::ResourceDescriptor& descriptor)
{
    SystemContext* systemContext = nullptr;
    if (isCrossSystemResource(descriptor))
    {
        const QString cloudSystemId = crossSystemResourceSystemId(descriptor);
        // TODO: #sivanov Probably it worth improve systemContextByCloudSystemId instead.
        if (cloudSystemId == kGenericCloudSystemId)
            systemContext = appContext()->cloudLayoutsSystemContext();
        else
            systemContext = appContext()->systemContextByCloudSystemId(cloudSystemId);
    }

    if (!systemContext)
        systemContext = appContext()->currentSystemContext();

    if (!NX_ASSERT(systemContext))
        return {};

    const auto resource = systemContext->resourcePool()->getResourceByDescriptor(descriptor);

    if (resource)
        return resource;

    // There are resources that are not cross-system resources but exist together with cloud
    // layouts. Therefore, it is necessary to check for the presence of such resources in cloud
    // layout system context resource pool. For example web pages and local files.
    systemContext = appContext()->cloudLayoutsSystemContext();
    if (systemContext)
        return systemContext->resourcePool()->getResourceByDescriptor(descriptor);

    return {};
}

bool isCrossSystemResource(const nx::vms::common::ResourceDescriptor& descriptor)
{
    const QString systemId = crossSystemResourceSystemId(descriptor);
    return !systemId.isEmpty()
        && systemId != appContext()->currentSystemContext()->moduleInformation().cloudSystemId;
}

QString crossSystemResourceSystemId(const nx::vms::common::ResourceDescriptor& descriptor)
{
    if (!descriptor.path.startsWith(kCloudScheme))
        return {};

    return descriptor.path.mid(
        kCloudScheme.length(),
        descriptor.path.indexOf('.') - kCloudScheme.length());
}

} // namespace nx::vms::client::desktop
