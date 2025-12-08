// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_layouts_source.h"

#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/cross_system/cloud_layouts_manager.h>
#include <nx/vms/client/core/cross_system/cross_system_layout_resource.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::core {
namespace entity_resource_tree {

CloudLayoutsSource::CloudLayoutsSource()
{
    const auto resourcePool = appContext()->cloudLayoutsManager()->systemContext()->resourcePool();

    connect(resourcePool, &QnResourcePool::resourcesAdded, this,
        [this](const QnResourceList& resources)
        {
            for (const auto& layout: resources.filtered<CrossSystemLayoutResource>())
                emit resourceAdded(layout);
        });

    connect(resourcePool, &QnResourcePool::resourcesRemoved, this,
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
        result.push_back(layout);

    return result;
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::core
