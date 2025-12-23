// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "unified_resource_pool.h"

#include <QtCore/QEventLoop>

#include <core/resource_management/resource_pool.h>
#include <nx/build_info.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/system_finder/system_finder.h>

namespace nx::vms::client::core {

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

QnResourceList UnifiedResourcePool::resources(const nx::Uuid& resourceId) const
{
    QnResourceList result;
    for (const auto& systemContext: appContext()->systemContexts())
    {
        if (auto resource = systemContext->resourcePool()->getResourceById(resourceId))
            result.append(resource);
    }
    return result;
}

QnResourcePtr UnifiedResourcePool::resource(
    const nx::Uuid& resourceId,
    const nx::Uuid& localSystemId) const
{
    const std::vector<SystemContext*> systemContexts = appContext()->systemContexts();
    const auto systemContext = std::find_if(systemContexts.begin(), systemContexts.end(),
        [localSystemId](const SystemContext* systemContext)
        {
            return systemContext->localSystemId() == localSystemId;
        });

    if (systemContext != systemContexts.end())
        return (*systemContext)->resourcePool()->getResourceById(resourceId);

    for (const auto& cloudId: appContext()->cloudCrossSystemManager()->cloudSystems())
    {
        auto cloudSystem = appContext()->cloudCrossSystemManager()->systemContext(cloudId);
        if (cloudSystem->systemDescription()->localId() == localSystemId)
        {
            // Here we need to suspend the execution of the method so that the scheduler can
            // asynchronously, outside the GUI thread, initiate a connection to the system,
            // which will then request the user confirmation in the GUI thread.
            //
            // This outcomes from the fact that this method is sync and initializeConnection works
            // in async manner and we have to wait for the connection to be completed to get
            // the resource.
            QEventLoop loop;
            QnResourcePtr result;

            cloudSystem->initializeConnection(/*allowUserInteraction*/ true);

            const nx::utils::ScopedConnection connectionGuard =
                connect(cloudSystem, &CloudCrossSystemContext::statusChanged, this,
                    [&loop, &result, &cloudSystem, &resourceId]
                    {
                        result = cloudSystem->systemContext()->resourcePool()
                            ->getResourceById(resourceId);
                        loop.quit();
                    });

            loop.exec();
            return result;
        }
    }
    return {};
}

} // namespace nx::vms::client::core
