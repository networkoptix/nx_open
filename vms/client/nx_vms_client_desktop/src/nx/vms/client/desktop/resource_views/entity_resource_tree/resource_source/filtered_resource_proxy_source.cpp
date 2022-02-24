// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "filtered_resource_proxy_source.h"

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

FilteredResourceProxySource::FilteredResourceProxySource(
    const std::function<bool(const QnResourcePtr&)>& resourceFilter,
    std::unique_ptr<AbstractResourceSource> baseResourceSource)
    :
    base_type(),
    m_resourceFilter(resourceFilter),
    m_baseResourceSource(std::move(baseResourceSource))
{
    connect(m_baseResourceSource.get(), &AbstractResourceSource::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            if (!m_resourceFilter || m_resourceFilter(resource))
                emit resourceAdded(resource);
        });

    connect(m_baseResourceSource.get(), &AbstractResourceSource::resourceRemoved, this,
        [this](const QnResourcePtr& resource) { emit resourceRemoved(resource); });
}

FilteredResourceProxySource::~FilteredResourceProxySource()
{
}

QVector<QnResourcePtr> FilteredResourceProxySource::getResources()
{
    const auto resources = m_baseResourceSource->getResources();
    if (!m_resourceFilter)
        return resources;

    QVector<QnResourcePtr> result;
    std::copy_if(std::cbegin(resources), std::cend(resources),
        std::back_inserter(result), m_resourceFilter);

    return result;
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
