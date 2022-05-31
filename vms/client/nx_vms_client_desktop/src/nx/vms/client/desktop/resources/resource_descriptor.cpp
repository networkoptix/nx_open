// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_descriptor.h"

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

namespace {

static const QString kCloudScheme = "cloud://";

QString resourcePath(const QnResourcePtr& resource)
{
    if (resource->hasFlags(Qn::exported_layout))
        return resource->getUrl();

    if (resource->hasFlags(Qn::local_video))
        return resource->getUrl();

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
    if (descriptor.path.startsWith(kCloudScheme))
    {
        QString cloudSystemId = descriptor.path.mid(kCloudScheme.length());
        cloudSystemId = cloudSystemId.mid(0, cloudSystemId.indexOf('.'));
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

} // namespace nx::vms::client::desktop
