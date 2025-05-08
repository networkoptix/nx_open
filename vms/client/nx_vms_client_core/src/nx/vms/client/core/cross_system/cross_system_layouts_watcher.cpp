// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_layouts_watcher.h"

#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/cross_system/cross_system_layout_resource.h>
#include <nx/vms/client/core/resource/resource_descriptor_helpers.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::core {

CrossSystemLayoutsWatcher::CrossSystemLayoutsWatcher(QObject* parent):
    QObject(parent)
{
    auto cloudLayoutsResourcePool = appContext()->cloudLayoutsSystemContext()->resourcePool();

    auto processLayouts =
        [cloudLayoutsResourcePool](CloudCrossSystemContext* context, const QString& systemId)
        {
            for (const auto& layout: cloudLayoutsResourcePool->getResources<LayoutResource>())
            {
                const bool wasModifiedByUser = layout->canBeSaved();
                const auto items = layout->getItems(); //< Iterate over local copy.
                for (const auto& item: items)
                {
                    if (isCrossSystemResource(item.resource)
                        && (crossSystemResourceSystemId(item.resource) == systemId))
                    {
                        layout->removeItem(item.uuid);
                        if (context && context->systemContext()->resourcePool()
                            ->getResourceByDescriptor(item.resource))
                        {
                            layout->addItem(item);
                        }
                    }
                }
                if (!wasModifiedByUser && layout->canBeSaved())
                    layout->storeSnapshot();
            }
        };

    connect(appContext()->cloudCrossSystemManager(),
        &CloudCrossSystemManager::systemFound,
        this,
        [this, processLayouts](const QString& systemId)
        {
            auto context = appContext()->cloudCrossSystemManager()->systemContext(systemId);
            if (!NX_ASSERT(context))
                return;

            connect(context,
                &CloudCrossSystemContext::statusChanged,
                this,
                [context, systemId, processLayouts]()
                {
                    // Handle system went online and loaded all it's cameras.
                    if (context->status() == CloudCrossSystemContext::Status::connected)
                        processLayouts(context, systemId);
                });
        });

    // This can happen either if a system was removed from the cloud, or if we logged out from the
    // cloud (and are about to get disconnected from the main system). In that case the system's
    // system context was deleted right before this signal was sent.
    // On contrary, if another system goes offline or unauthorized, it's not considered lost,
    // and cloudCrossSystemManager keeps its system context intact.
    connect(appContext()->cloudCrossSystemManager(),
        &CloudCrossSystemManager::systemLost,
        this,
        [this, processLayouts](const QString& systemId)
        {
            NX_ASSERT(!appContext()->cloudCrossSystemManager()->systemContext(systemId));
            processLayouts(nullptr, systemId);
        });

    if (ini().validateCloudLayouts)
    {
        auto validate = [](
            const QnLayoutResourcePtr& /*layout*/, const common::LayoutItemData& item)
            {
                auto resource = getResourceByDescriptor(item.resource);
                if (resource && resource->hasFlags(Qn::local_media))
                    return;

                if (!item.resource.path.isEmpty())
                    return;

                NX_ASSERT(!crossSystemResourceSystemId(item.resource).isEmpty(),
                    "Disable `validateCloudLayouts` in the `desktop_client.ini` if get "
                    "this assert on the client start, then delete misconstructed cloud layouts");
            };

        // Validation mechanism to ensure cloud layouts contain only cross-system resources.
        connect(cloudLayoutsResourcePool, &QnResourcePool::resourceAdded, this,
            [this, validate](const QnResourcePtr& resource)
            {
                if (auto layout = resource.dynamicCast<LayoutResource>())
                {
                    connect(layout.data(), &LayoutResource::itemAdded, this, validate);
                    connect(layout.data(), &LayoutResource::itemChanged, this, validate);
                    for (const auto& item: layout->getItems())
                        validate(layout, item);
                }
            });

        connect(cloudLayoutsResourcePool, &QnResourcePool::resourceRemoved, this,
            [this](const QnResourcePtr& resource)
            {
                if (auto layout = resource.dynamicCast<LayoutResource>())
                    layout->disconnect(this);
            });
    }
}

} // namespace nx::vms::client::core
