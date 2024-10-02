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
    const auto resourcePool = appContext()->cloudLayoutsManager()->systemContext()->resourcePool();

    connect(resourcePool, &QnResourcePool::resourcesAdded,
        [this](const QnResourceList& resources)
        {
            for (const auto& layout: resources.filtered<CrossSystemLayoutResource>())
            {
                if (layout->hasFlags(Qn::local))
                    listenLocalLayout(layout);
                else
                    emit resourceAdded(layout);
            }
        });

    connect(resourcePool, &QnResourcePool::resourcesRemoved,
        [this](const QnResourceList& resources)
        {
            for (const auto& layout: resources.filtered<CrossSystemLayoutResource>())
                emit resourceRemoved(layout);
        });
}

QVector<QnResourcePtr> CloudLayoutsSource::getResources()
{
    const auto resourcePool = appContext()->cloudLayoutsManager()->systemContext()->resourcePool();

    QVector<QnResourcePtr> result;
    for (const auto& layout: resourcePool->getResources<CrossSystemLayoutResource>())
    {
        if (layout->hasFlags(Qn::local))
            listenLocalLayout(layout);
        else
            result.push_back(layout);
    }

    return result;
}

void CloudLayoutsSource::processLocalLayout(const QnResourcePtr& resource)
{
    const auto layout = resource.dynamicCast<CrossSystemLayoutResource>();
    // Check removed flag as we do not do disconnect on remove.
    if (NX_ASSERT(layout)
        && !layout->hasFlags(Qn::local)
        && !layout->hasFlags(Qn::removed))
    {
        layout->disconnect(this);
        emit resourceAdded(layout);
    }
}

void CloudLayoutsSource::listenLocalLayout(const CrossSystemLayoutResourcePtr& layout)
{
    connect(layout.data(), &QnResource::flagsChanged,
        this, &CloudLayoutsSource::processLocalLayout);
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
