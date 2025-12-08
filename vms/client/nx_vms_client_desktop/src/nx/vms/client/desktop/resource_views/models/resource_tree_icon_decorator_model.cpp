// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_icon_decorator_model.h"

#include <nx/vms/client/core/skin/resource_icon_cache.h>
#include <nx/vms/client/desktop/application_context.h>

#include <nx/utils/log/assert.h>
#include <client/client_globals.h>

namespace nx::vms::client::desktop {

ResourceTreeIconDecoratorModel::ResourceTreeIconDecoratorModel():
    base_type(nullptr),
    m_resourceIconCache(appContext()->resourceIconCache())
{
    NX_ASSERT(m_resourceIconCache);
}

ResourceTreeIconDecoratorModel::~ResourceTreeIconDecoratorModel()
{
}

bool ResourceTreeIconDecoratorModel::displayResourceStatus() const
{
    return m_displayResourceStatus;
}

void ResourceTreeIconDecoratorModel::setDisplayResourceStatus(bool display)
{
    m_displayResourceStatus = display;
}

QVariant ResourceTreeIconDecoratorModel::data(const QModelIndex& index, int role) const
{
    using nx::vms::client::core::ResourceIconCache;

    if (role == Qt::DecorationRole)
    {
        auto iconKeyData = sourceModel()->data(mapToSource(index), core::ResourceIconKeyRole);
        if (!iconKeyData.isNull())
        {
            auto iconKey = static_cast<ResourceIconCache::Key>(iconKeyData.toInt());
            if (!displayResourceStatus())
                iconKey &= ~ResourceIconCache::StatusMask;

            return m_resourceIconCache->icon(iconKey);
        }
    }
    return base_type::data(index, role);
}

} // namespace nx::vms::client::desktop
