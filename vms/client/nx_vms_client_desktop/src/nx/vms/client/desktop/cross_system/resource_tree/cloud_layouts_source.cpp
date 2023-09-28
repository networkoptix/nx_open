// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_layouts_source.h"

#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>

#include "../cloud_layouts_manager.h"
#include "../cross_system_layout_resource.h"

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

CloudLayoutsSource::CloudLayoutsSource()
{
    initializeRequest =
        [this]
        {
            const auto cloudLayoutsManager = appContext()->cloudLayoutsManager();
            const auto resourcePool = cloudLayoutsManager->systemContext()->resourcePool();

            auto processLocalLayout =
                [this](const QnResourcePtr& resource)
                {
                    auto layout = resource.dynamicCast<CrossSystemLayoutResource>();
                    // Check removed flag as we do not do disconnect on remove.
                    if (NX_ASSERT(layout)
                        && !layout->hasFlags(Qn::local)
                        && !layout->hasFlags(Qn::removed))
                    {
                        (*addKeyHandler)(layout);
                    }
                };

            auto listenLocalLayout =
                [this, processLocalLayout](const CrossSystemLayoutResourcePtr& layout)
                {
                    m_connectionsGuard.add(QObject::connect(
                        layout.data(),
                        &QnResource::flagsChanged,
                        processLocalLayout));
                };

            m_connectionsGuard.add(QObject::connect(
                resourcePool, &QnResourcePool::resourcesAdded,
                    [this, listenLocalLayout](const QnResourceList& resources)
                    {
                        for (const auto& layout: resources.filtered<CrossSystemLayoutResource>())
                        {
                            if (layout->hasFlags(Qn::local))
                                listenLocalLayout(layout);
                            else
                                (*addKeyHandler)(layout);
                        }
                    }));

            m_connectionsGuard.add(QObject::connect(
                resourcePool, &QnResourcePool::resourcesRemoved,
                    [this](const QnResourceList& resources)
                    {
                        for (const auto& layout: resources.filtered<CrossSystemLayoutResource>())
                            (*removeKeyHandler)(layout);
                    }));

            QVector<QnResourcePtr> keys;
            for (const auto& layout: resourcePool->getResources<CrossSystemLayoutResource>())
            {
                if (layout->hasFlags(Qn::local))
                    listenLocalLayout(layout);
                else
                    keys.push_back(layout);
            }
            setKeysHandler(keys);
        };
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
