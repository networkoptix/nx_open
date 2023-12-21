// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_layouts_watcher.h"

#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/system_context.h>

#include "cloud_cross_system_context.h"
#include "cloud_cross_system_manager.h"
#include "cross_system_layout_resource.h"

namespace nx::vms::client::desktop {

CrossSystemLayoutsWatcher::CrossSystemLayoutsWatcher(QObject* parent):
    QObject(parent)
{
    auto cloudLayoutsResourcePool = appContext()->cloudLayoutsSystemContext()->resourcePool();

    auto processLayouts =
        [cloudLayoutsResourcePool](CloudCrossSystemContext* context, const QString& systemId)
        {
            const auto snapshotManager = appContext()->cloudLayoutsSystemContext()
                ->layoutSnapshotManager();

            for (const auto& layout: cloudLayoutsResourcePool->getResources<LayoutResource>())
            {
                const bool wasModifiedByUser = snapshotManager->isModified(layout);
                const auto items = layout->getItems(); //< Iterate over local copy.
                for (const auto& item: items)
                {
                    if (isCrossSystemResource(item.resource)
                        && (crossSystemResourceSystemId(item.resource) == systemId))
                    {
                        layout->removeItem(item.uuid);
                        if (context->systemContext()->resourcePool()
                            ->getResourceByDescriptor(item.resource))
                        {
                            layout->addItem(item);
                        }
                    }
                }
                if (!wasModifiedByUser && snapshotManager->isModified(layout))
                    snapshotManager->store(layout);
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
                    "Disable `validateCloudLayouts` in the `nx_vms_client_desktop.ini` if get "
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

} // namespace nx::vms::client::desktop
