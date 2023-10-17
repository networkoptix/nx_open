// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "accessible_resource_proxy_source.h"

#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using namespace nx::core::access;

AccessibleResourceProxySource::AccessibleResourceProxySource(
    SystemContext* systemContext,
    const QnResourceAccessSubject& accessSubject,
    std::unique_ptr<AbstractResourceSource> baseResourceSource)
    :
    base_type(),
    SystemContextAware(systemContext),
    m_accessSubject(accessSubject),
    m_baseResourceSource(std::move(baseResourceSource))
{
    connect(m_baseResourceSource.get(), &AbstractResourceSource::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            if (hasAccess(resource))
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
            m_deniedResources.remove(resource);

            if (m_acceptedResources.remove(resource))
                emit resourceRemoved(resource);
        });

    const auto notifier = systemContext->resourceAccessManager()->createNotifier();
    notifier->setSubjectId(accessSubject.id());

    connect(notifier, &QnResourceAccessManager::Notifier::resourceAccessChanged, this,
        [this]()
        {
            QnResourceList newlyAccepted;
            QnResourceList newlyDenied;

            for (const auto& resource: m_acceptedResources)
            {
                if (!hasAccess(resource))
                    newlyDenied.push_back(resource);
            }

            for (const auto& resource: m_deniedResources)
            {
                if (hasAccess(resource))
                    newlyAccepted.push_back(resource);
            }

            for (const auto& resource: newlyDenied)
            {
                m_acceptedResources.remove(resource);
                m_deniedResources.insert(resource);
                emit resourceRemoved(resource);
            }

            for (const auto& resource: newlyAccepted)
            {
                m_deniedResources.remove(resource);
                m_acceptedResources.insert(resource);
                emit resourceAdded(resource);
            }
        });

    // TODO: #sivanov There should be more elegant way to handle unit tests limitations.
    // Message processor does not exist in unit tests.
    if (messageProcessor())
    {
        auto cachesCleaner = new core::SessionResourcesSignalListener<QnResource>(
            systemContext,
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
        [this](const auto& resource) { return hasAccess(resource); });

    m_acceptedResources = QSet<QnResourcePtr>(result.begin(), partitionItr);
    m_deniedResources = QSet<QnResourcePtr>(partitionItr, result.end());

    result.erase(partitionItr, result.end());
    return result;
}

bool AccessibleResourceProxySource::hasAccess(const QnResourcePtr& resource) const
{
    if (!NX_ASSERT(resource && resource->systemContext() == systemContext()))
        return false;

    const auto requiredPermission = resource->hasFlags(Qn::server)
        ? Qn::ViewContentPermission
        : Qn::ReadPermission;

    const auto accessManager = systemContext()->resourceAccessManager();
    return accessManager->hasPermission(m_accessSubject, resource, requiredPermission);
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
