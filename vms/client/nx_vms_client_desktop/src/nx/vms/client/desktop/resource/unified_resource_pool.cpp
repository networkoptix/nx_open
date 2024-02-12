// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "unified_resource_pool.h"

#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

UnifiedResourcePool::UnifiedResourcePool(QObject* parent):
    QObject(parent)
{
    auto connectToContext =
        [this](SystemContext* systemContext)
        {
            connect(systemContext->resourcePool(), &QnResourcePool::resourcesAdded,
                this, &UnifiedResourcePool::resourcesAdded);
            connect(systemContext->resourcePool(), &QnResourcePool::resourcesRemoved,
                this, &UnifiedResourcePool::resourcesRemoved);

            auto resources = systemContext->resourcePool()->getResources();
            if (!resources.empty())
                emit resourcesAdded(resources);
        };

    auto disconnectFromContext =
        [this](SystemContext* systemContext)
        {
            systemContext->resourcePool()->disconnect(this);
            auto resources = systemContext->resourcePool()->getResources();
            if (!resources.empty())
                emit resourcesRemoved(resources);
        };

    connect(appContext(), &ApplicationContext::systemContextAdded,
        this, connectToContext);

    connect(appContext(), &ApplicationContext::systemContextRemoved,
        this, disconnectFromContext);

    for (auto context: appContext()->systemContexts())
        connectToContext(context);
}

QnResourceList UnifiedResourcePool::resources(ResourceFilter filter) const
{
    QnResourceList result;
    for (const auto& systemContext: appContext()->systemContexts())
        result.append(systemContext->resourcePool()->getResources(filter));
    return result;
}

QnResourcePtr UnifiedResourcePool::resource(const nx::Uuid& id) const
{
    for (const auto& systemContext: appContext()->systemContexts())
    {
        if (const auto resource = systemContext->resourcePool()->getResourceById(id))
            return resource;
    }
    return {};
}

} // namespace nx::vms::client::desktop
