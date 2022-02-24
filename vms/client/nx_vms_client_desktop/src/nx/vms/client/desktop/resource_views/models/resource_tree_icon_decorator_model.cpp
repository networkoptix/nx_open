// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_icon_decorator_model.h"

#include <nx/vms/client/desktop/style/resource_icon_cache.h>

#include <nx/utils/log/assert.h>
#include <client/client_globals.h>

namespace nx::vms::client::desktop {

ResourceTreeIconDecoratorModel::ResourceTreeIconDecoratorModel():
    base_type(nullptr),
    m_resourceIconCache(qnResIconCache)
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
    if (role == Qt::DecorationRole)
    {
        auto iconKeyData = sourceModel()->data(mapToSource(index), Qn::ResourceIconKeyRole);
        if (!iconKeyData.isNull())
        {
            auto iconKey = static_cast<QnResourceIconCache::Key>(iconKeyData.toInt());
            if (!displayResourceStatus())
                iconKey &= ~QnResourceIconCache::StatusMask;

            return m_resourceIconCache->icon(iconKey);
        }
    }
    return base_type::data(index, role);
}

} // namespace nx::vms::client::desktop
