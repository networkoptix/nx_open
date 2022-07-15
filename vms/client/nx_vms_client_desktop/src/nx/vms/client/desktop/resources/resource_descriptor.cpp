// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_descriptor.h"

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_layouts_manager.h>
#include <nx/vms/client/desktop/cross_system/cross_system_layout_resource.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

namespace {

static const QString kCloudScheme = "cloud://";

/** System Id placeholder for resources, not bound to any System (e.g. cloud layouts). */
static const QString kGenericCloudSystemId = QnUuid().toSimpleString();

QString resourcePath(const QnResourcePtr& resource)
{
    if (resource->hasFlags(Qn::exported_layout))
        return resource->getUrl();

    if (resource->hasFlags(Qn::local_video))
        return resource->getUrl();

    if (resource.dynamicCast<CrossSystemLayoutResource>())
    {
        return nx::format(kCloudScheme + "%1.%2",
            kGenericCloudSystemId,
            resource->getId().toSimpleString());
    }

    auto systemContext = SystemContext::fromResource(resource);
    if (NX_ASSERT(systemContext))
    {
        if (auto cloudSystemId = systemContext->moduleInformation().cloudSystemId;
            !cloudSystemId.isEmpty())
        {
            return nx::format(kCloudScheme + "%1.%2",
                cloudSystemId,
                resource->getId().toSimpleString());
        }
    }
    return {};
}

} // namespace

/** Create resource descriptor for storing in layout item or passing to another client instance. */
nx::vms::common::ResourceDescriptor descriptor(const QnResourcePtr& resource)
{
    if (!NX_ASSERT(resource))
        return {};

    return {resource->getId(), resourcePath(resource)};
}

/** Find Resource in a corresponding System Context. */
QnResourcePtr getResourceByDescriptor(const nx::vms::common::ResourceDescriptor& descriptor)
{
    SystemContext* systemContext = nullptr;
    if (ini().crossSystemLayouts && isCrossSystemResource(descriptor))
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

    return systemContext->resourcePool()->getResourceByDescriptor(descriptor);
}

bool isCrossSystemResource(const nx::vms::common::ResourceDescriptor& descriptor)
{
    return descriptor.path.startsWith(kCloudScheme);
}

QString crossSystemResourceSystemId(const nx::vms::common::ResourceDescriptor& descriptor)
{
    NX_ASSERT(isCrossSystemResource(descriptor));
    return descriptor.path.mid(
        kCloudScheme.length(),
        descriptor.path.indexOf('.') - kCloudScheme.length());
}

} // namespace nx::vms::client::desktop
