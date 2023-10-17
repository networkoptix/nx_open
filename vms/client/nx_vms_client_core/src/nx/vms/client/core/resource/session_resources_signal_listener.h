// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::core {

/**
 * Utility for the coinvinient handling of the resources changes. Intended for use when we need to
 * listen some signals from all resources of a kind and only while the client is connected to a
 * System. Useful when something is to be recalculated depending on all certain resources data.
 *
 * Example: we need to show a notification when some users have invalid e-mail addresses. So we
 * need to update notification state each time when a user is added or removed, and also each time
 * when any user's e-mail is changed and when any user is enabled or disabled.
 *
 * After listener is set, `start()` method should be called manually to make sure all handlers are
 * ready.
 */
template<typename ResourceClass>
class SessionResourcesSignalListener: public QObject, public SystemContextAware
{
    using base_type = QObject;
    using ResourcePtr = QnSharedResourcePointer<ResourceClass>;
    using ResourceList = QnSharedResourcePointerList<ResourceClass>;
    using ResourceHandler = std::function<void(const ResourcePtr&)>;
    using ResourceProperyChangedHandler = std::function<void(const ResourcePtr&, const QString&)>;
    using ResourceListHandler = std::function<void(const ResourceList&)>;

public:
    SessionResourcesSignalListener(
        SystemContext* systemContext,
        QObject* parent = nullptr)
        :
        base_type(parent),
        SystemContextAware(systemContext)
    {
    }

    /**
     * Should be called after all handlers are set. If there are corresponding resources in the
     * resources pool, `onAdded` handler will be called immediately.
     */
    void start()
    {
        establishResourcePoolConnections();
    }

    /** Stop handling resources changes. */
    void stop()
    {
        m_connections.reset();
    }

    void setOnAddedHandler(ResourceListHandler handler)
    {
        m_onAddedHandler = handler;
    }

    void setOnRemovedHandler(ResourceListHandler handler)
    {
        m_onRemovedHandler = handler;
    }

    template<typename ResourceSignalClass>
    void addOnSignalHandler(ResourceSignalClass signal, ResourceHandler handler)
    {
        const ResourceHandler bindToSignal =
            [=](const ResourcePtr& resource)
            {
                m_connections << connect(
                    resource.get(),
                    signal,
                    this,
                    [resource, handler]() { handler(resource); });
            };
        m_onSignalHandlers << bindToSignal;
    }

    void addOnPropertyChangedHandler(ResourceProperyChangedHandler handler)
    {
        const ResourceHandler bindToSignal =
            [=](const ResourcePtr& resource)
            {
                m_connections << connect(
                    resource.get(),
                    &QnResource::propertyChanged,
                    this,
                    [resource, handler](const QnResourcePtr& /*self*/, const QString& key)
                    {
                        handler(resource, key);
                    });
            };
        m_onSignalHandlers << bindToSignal;
    }

    template<typename ResourceSignalClass, typename ResourceHandlerClass>
    void addOnCustomSignalHandler(ResourceSignalClass signal, ResourceHandlerClass handler)
    {
        const ResourceHandler bindToSignal =
            [=](const ResourcePtr& resource)
            {
                m_connections << connect(
                    resource.get(),
                    signal,
                    this,
                    handler);
            };
        m_onSignalHandlers << bindToSignal;
    }

private:
    ResourceList filterByClass(const QnResourceList& resources)
    {
        if constexpr (std::is_same_v<ResourceClass, QnResource>)
            return resources;

        return resources.filtered<ResourceClass>();
    }

    void establishResourcePoolConnections()
    {
        m_connections.reset();

        auto processAddedResources =
            [this](const QnResourceList& resources)
            {
                if (m_onAddedHandler || !m_onSignalHandlers.empty())
                {
                    ResourceList filtered = filterByClass(resources);
                    if (filtered.empty())
                        return;

                    for (const auto& bindToSignal: m_onSignalHandlers)
                    {
                        for (const ResourcePtr& resource: std::as_const(filtered))
                            bindToSignal(resource);
                    }

                    if (m_onAddedHandler)
                        m_onAddedHandler(filtered);
                }
            };

        processAddedResources(resourcePool()->getResources());

        m_connections << connect(resourcePool(), &QnResourcePool::resourcesAdded, this,
            processAddedResources);

        m_connections << connect(resourcePool(), &QnResourcePool::resourcesRemoved, this,
            [this](const QnResourceList& resources)
            {
                for (const auto& resource: resources)
                    resource->disconnect(this);

                if (m_onRemovedHandler)
                {
                    ResourceList filtered = filterByClass(resources);
                    if (!filtered.empty())
                        m_onRemovedHandler(filtered);
                }
            });
    }

private:
    nx::utils::ScopedConnections m_connections;

    ResourceListHandler m_onAddedHandler;
    ResourceListHandler m_onRemovedHandler;
    QList<ResourceHandler> m_onSignalHandlers;
};

} // namespace nx::vms::client::core
