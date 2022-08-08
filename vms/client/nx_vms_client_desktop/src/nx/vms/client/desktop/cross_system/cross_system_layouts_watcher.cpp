// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_layouts_watcher.h"

#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resources/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/resources/resource_descriptor.h>
#include <nx/vms/client/desktop/system_context.h>

#include "cloud_cross_system_context.h"
#include "cloud_cross_system_manager.h"
#include "cross_system_layout_resource.h"

namespace nx::vms::client::desktop {

CrossSystemLayoutsWatcher::CrossSystemLayoutsWatcher(QObject* parent):
    QObject(parent)
{
    auto processLayouts =
        [](CloudCrossSystemContext* context, const QString& systemId)
        {
            const auto snapshotManager = appContext()->cloudLayoutsSystemContext()
                ->layoutSnapshotManager();

            for (const auto& layout: appContext()->cloudLayoutsSystemContext()->resourcePool()
                ->getResources<QnLayoutResource>())
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
                    if (context->isConnected())
                        processLayouts(context, systemId);
                });
        });
}

} // namespace nx::vms::client::desktop
