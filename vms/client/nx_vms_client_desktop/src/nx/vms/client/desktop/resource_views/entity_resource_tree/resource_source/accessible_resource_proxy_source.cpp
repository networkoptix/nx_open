// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "accessible_resource_proxy_source.h"

#include <core/resource_access/providers/resource_access_provider.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using namespace nx::core::access;

AccessibleResourceProxySource::AccessibleResourceProxySource(
    SystemContext* systemContext,
    const ResourceAccessProvider* accessProvider,
    const QnResourceAccessSubject& accessSubject,
    std::unique_ptr<AbstractResourceSource> baseResourceSource)
    :
    base_type(),
    SystemContextAware(systemContext),
    m_accessProvider(accessProvider),
    m_accessSubject(accessSubject),
    m_baseResourceSource(std::move(baseResourceSource))
{
    connect(m_baseResourceSource.get(), &AbstractResourceSource::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            if (m_accessProvider->hasAccess(m_accessSubject, resource))
            {
                m_acceptedResources.insert(resource);
                emit resourceAdded(resource);
            }
            else
            {
                m_deniedResources.insert(resource);
            }
        });

    connect(m_baseResourceSource.get(), &AbstractResourceSource::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            m_acceptedResources.remove(resource);
            m_deniedResources.remove(resource);
            emit resourceRemoved(resource);
        });

    connect(m_accessProvider, &ResourceAccessProvider::accessChanged, this,
        [this](const QnResourceAccessSubject& subject, const QnResourcePtr& resource)
        {
            if (subject != m_accessSubject || resource->hasFlags(Qn::removed))
                return;

            const bool hasAccess = m_accessProvider->hasAccess(subject, resource);
            if (hasAccess && m_deniedResources.contains(resource))
            {
                m_deniedResources.remove(resource);
                m_acceptedResources.insert(resource);
                emit resourceAdded(resource);
            }
            if (!hasAccess && m_acceptedResources.contains(resource))
            {
                m_acceptedResources.remove(resource);
                m_deniedResources.insert(resource);
                emit resourceRemoved(resource);
            }
        });

    // Message processor does not exist in unit tests.
    if (auto messageProcessor = this->messageProcessor())
    {
        auto cachesCleaner = new core::SessionResourcesSignalListener<QnResource>(
            resourcePool(),
            messageProcessor,
            this);
        cachesCleaner->setOnRemovedHandler(
            [this](const QnResourceList& resources)
            {
                for (const auto& resource: resources)
                {
                    if (m_accessSubject.user() == resource)
                        m_accessSubject = {};

                    m_acceptedResources.remove(resource);
                    m_deniedResources.remove(resource);
                }
            });
        cachesCleaner->start();
    }
}

AccessibleResourceProxySource::~AccessibleResourceProxySource()
{
}

QVector<QnResourcePtr> AccessibleResourceProxySource::getResources()
{
    QVector<QnResourcePtr> result(m_baseResourceSource->getResources());
    const auto partitionItr = std::partition(result.begin(), result.end(),
        [this](const auto& resource)
        {
            return m_accessProvider->hasAccess(m_accessSubject, resource);
        });

    m_acceptedResources = QSet<QnResourcePtr>(result.begin(), partitionItr);
    m_deniedResources = QSet<QnResourcePtr>(partitionItr, result.end());

    result.erase(partitionItr, result.end());
    return result;
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
