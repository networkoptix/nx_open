// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "parent_servers_proxy_source.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

ParentServersProxySource::ParentServersProxySource(AbstractResourceSourcePtr resourceSource):
    m_resourceSource(std::move(resourceSource))
{
    const auto resources = m_resourceSource->getResources();
    for (const auto& resource: resources)
    {
        if (const auto server = resource->getParentResource().dynamicCast<QnMediaServerResource>())
            m_resourcesByServer[server].insert(resource);

        connect(resource.get(), &QnResource::parentIdChanged,
            this, &ParentServersProxySource::onResourceParentIdChanged);
    }

    connect(m_resourceSource.get(), &AbstractResourceSource::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            connect(resource.get(), &QnResource::parentIdChanged,
                this, &ParentServersProxySource::onResourceParentIdChanged, Qt::UniqueConnection);
            mapResource(resource);
        });

    connect(m_resourceSource.get(), &AbstractResourceSource::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            resource->disconnect(this);
            unmapResource(resource, resource->getParentResource());
        });
}

QVector<QnResourcePtr> ParentServersProxySource::getResources()
{
    QVector<QnResourcePtr> result;
    result.reserve(m_resourcesByServer.size());

    for (auto itr = m_resourcesByServer.cbegin(); itr != m_resourcesByServer.cend(); ++itr)
        result.push_back(itr.key());

    return result;
}

void ParentServersProxySource::onResourceParentIdChanged(
    const QnResourcePtr& resource,
    const nx::Uuid& previousParentId)
{
    unmapResource(resource, resource->resourcePool()->getResourceById(previousParentId));
    mapResource(resource);
}

void ParentServersProxySource::mapResource(const QnResourcePtr& resource)
{
    if (const auto server = resource->getParentResource().dynamicCast<QnMediaServerResource>())
    {
        const bool firstMapped = !m_resourcesByServer.contains(server);
        m_resourcesByServer[server].insert(resource);
        if (firstMapped)
            emit resourceAdded(server);
    }
}

void ParentServersProxySource::unmapResource(
    const QnResourcePtr& resource,
    const QnResourcePtr& parentResource)
{
    if (const auto server = parentResource.dynamicCast<QnMediaServerResource>())
    {
        if (!m_resourcesByServer.contains(server))
            return;

        auto& resourcesOnServer = m_resourcesByServer[server];
        if (resourcesOnServer.remove(resource) && resourcesOnServer.empty())
        {
            m_resourcesByServer.remove(server);
            emit resourceRemoved(server);
        }
    }
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
