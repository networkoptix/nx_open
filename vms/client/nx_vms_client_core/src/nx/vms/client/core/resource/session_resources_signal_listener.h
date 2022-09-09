// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <api/common_message_processor.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/common/system_context.h>

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
 * After listener is set, `start()` method should be called manually. It is required to maintain
 * correctness if the class instance is constructed when the system connection is ready and active.
 * This method will do nothing if the client is not connected yet.
 *
 * Connection closing should be handled using a separate handler as the listener breaks all
 * resources pool connections instantly. There will be no separate `onRemoved()` call.
 */
template<typename ResourceClass>
class SessionResourcesSignalListener: public QObject
{
    using base_type = QObject;
    using ResourcePtr = QnSharedResourcePointer<ResourceClass>;
    using ResourceList = QnSharedResourcePointerList<ResourceClass>;
    using ResourceHandler = std::function<void(const ResourcePtr&)>;
    using ResourceProperyChangedHandler = std::function<void(const ResourcePtr&, const QString&)>;
    using ResourceListHandler = std::function<void(const ResourceList&)>;

public:
    SessionResourcesSignalListener(
        QnResourcePool* resourcePool,
        QnCommonMessageProcessor* messageProcessor,
        QObject* parent = nullptr)
        :
        base_type(parent),
        m_resourcePool(resourcePool),
        m_messageProcessor(messageProcessor)
    {
        NX_ASSERT(resourcePool);
        NX_ASSERT(messageProcessor);

        connect(messageProcessor, &QnCommonMessageProcessor::initialResourcesReceived, this,
            &SessionResourcesSignalListener::establishResourcePoolConnections);

        connect(messageProcessor, &QnCommonMessageProcessor::connectionClosed, this,
            [this]()
            {
                m_connections.reset();
                // TODO: #sivanov This code must be re-enabled after setOnConnectionClosedHandler()
                // is repaired.
                //if (m_onConnectionClosedHandler)
                //    m_onConnectionClosedHandler();
            });
    }

    SessionResourcesSignalListener(QObject* parent = nullptr):
        SessionResourcesSignalListener(
            qnClientCoreModule->resourcePool(),
            qnClientCoreModule->commonModule()->messageProcessor(),
            parent)
    {
    }

    /**
     * Should be called after all handlers are set. If the client is already connected to the
     * system, `onAdded` handler will be called immediately. Otherwise handlers will be called as
     * soon as connection is established.
     */
    void start()
    {
        // TODO: #sivanov More correct check is to verify RemoteSession state to ensure we did not
        // receive `initialResourcesReceived` yet.
        if (m_messageProcessor->connection())
            establishResourcePoolConnections();
    }

    void setOnAddedHandler(ResourceListHandler handler)
    {
        m_onAddedHandler = handler;
    }

    void setOnRemovedHandler(ResourceListHandler handler)
    {
        m_onRemovedHandler = handler;
    }

    // TODO: #sivanov Here it is necessary to carefully consider all scenarios when the connection can
    // be closed.
    //void setOnConnectionClosedHandler(ConnectionClosedHandler handler)
    //{
    //    m_onConnectionClosedHandler = handler;
    //}

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
        if (!NX_ASSERT(m_resourcePool))
            return;

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

        processAddedResources(m_resourcePool->getResources());

        m_connections << connect(m_resourcePool.data(), &QnResourcePool::resourcesAdded, this,
            processAddedResources);

        m_connections << connect(m_resourcePool.data(), &QnResourcePool::resourcesRemoved, this,
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
    QPointer<QnResourcePool> m_resourcePool;
    QPointer<QnCommonMessageProcessor> m_messageProcessor;
    nx::utils::ScopedConnections m_connections;

    ResourceListHandler m_onAddedHandler;
    ResourceListHandler m_onRemovedHandler;
    QList<ResourceHandler> m_onSignalHandlers;
};

} // namespace nx::vms::client::core
