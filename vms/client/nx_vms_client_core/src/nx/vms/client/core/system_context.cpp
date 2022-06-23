// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context.h"

#include <QtQml/QtQml>

#include <client/client_message_processor.h>
#include <core/ptz/client_ptz_controller_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/core/watchers/watermark_watcher.h>
#include <nx/vms/rules/engine_holder.h>
#include <nx/vms/rules/initializer.h>

namespace nx::vms::client::core {

namespace {

static const QString kSystemContextPropertyName("systemContext");

} // namespace

class SystemContextFromQmlInitializer: public nx::vms::common::SystemContextInitializer
{
public:
    SystemContextFromQmlInitializer(QObject* owner):
        m_owner(owner)
    {
    }

    virtual SystemContext* systemContext() const override
    {
        auto qmlContext = QQmlEngine::contextForObject(m_owner);

        while (NX_ASSERT(qmlContext))
        {
            auto systemContext = qmlContext->contextProperty(kSystemContextPropertyName);
            if (systemContext.isValid())
                return systemContext.value<SystemContext*>();

            qmlContext = qmlContext->parentContext();
        }

        return nullptr;
    }

private:
    QObject* const m_owner;
};

struct SystemContext::Private
{
    std::unique_ptr<QnClientPtzControllerPool> ptzControllerPool;
    std::unique_ptr<UserWatcher> userWatcher;
    std::unique_ptr<WatermarkWatcher> watermarkWatcher;
    std::unique_ptr<ServerTimeWatcher> serverTimeWatcher;
    std::unique_ptr<nx::vms::rules::EngineHolder> vmsRulesEngineHolder;

    mutable nx::Mutex sessionMutex;

    /** Session this context belongs to. Exclusive with connection. */
    std::shared_ptr<RemoteSession> session;

    /** Connection this context belongs to. Exclusive with session. */
    RemoteConnectionPtr connection;
};

SystemContext::SystemContext(QnUuid peerId, QObject* parent):
    base_type(
        std::move(peerId),
        /*sessionId*/ QnUuid::createUuid(),
        nx::core::access::Mode::cached,
        parent),
    d(new Private())
{
    d->ptzControllerPool = std::make_unique<QnClientPtzControllerPool>(this);
    d->userWatcher = std::make_unique<UserWatcher>(this);
    d->watermarkWatcher = std::make_unique<WatermarkWatcher>(this);
    d->serverTimeWatcher = std::make_unique<ServerTimeWatcher>(this);

    d->vmsRulesEngineHolder = std::make_unique<nx::vms::rules::EngineHolder>(
        this,
        std::make_unique<nx::vms::rules::Initializer>(this));
}

SystemContext::~SystemContext()
{
}

SystemContext* SystemContext::fromResource(const QnResourcePtr& resource)
{
    if (!resource)
        return {};

    return dynamic_cast<SystemContext*>(resource->systemContext());
}

std::unique_ptr<nx::vms::common::SystemContextInitializer> SystemContext::fromQmlContext(
    QObject* source)
{
    return std::make_unique<SystemContextFromQmlInitializer>(source);
}

void SystemContext::storeToQmlContext(QQmlContext* qmlContext)
{
    qmlContext->setContextProperty(kSystemContextPropertyName, this);
}

nx::vms::api::ModuleInformation SystemContext::moduleInformation() const
{
    if (auto connection = this->connection())
        return connection->moduleInformation();

    return {};
}

void SystemContext::setSession(std::shared_ptr<RemoteSession> session)
{
    {
        NX_MUTEX_LOCKER lock(&d->sessionMutex);
        NX_ASSERT(!d->connection);
        d->session = session;
    }
    emit remoteIdChanged(currentServerId());
}

void SystemContext::setConnection(RemoteConnectionPtr connection)
{
    NX_ASSERT(!d->connection);
    NX_ASSERT(!d->session);
    d->connection = connection;
}

QnUuid SystemContext::currentServerId() const
{
    return moduleInformation().id;
}

QnMediaServerResourcePtr SystemContext::currentServer() const
{
    return resourcePool()->getResourceById<QnMediaServerResource>(currentServerId());
}

std::shared_ptr<RemoteSession> SystemContext::session() const
{
    NX_MUTEX_LOCKER lock(&d->sessionMutex);
    return d->session;
}

RemoteConnectionPtr SystemContext::connection() const
{
    NX_MUTEX_LOCKER lock(&d->sessionMutex);
    if (d->session)
        return d->session->connection();

    return d->connection;
}

rest::ServerConnectionPtr SystemContext::connectedServerApi() const
{
    if (auto connection = this->connection())
        return connection->serverApi();

    return {};
}

QnPtzControllerPool* SystemContext::ptzControllerPool() const
{
    return d->ptzControllerPool.get();
}

UserWatcher* SystemContext::userWatcher() const
{
    return d->userWatcher.get();
}

WatermarkWatcher* SystemContext::watermarkWatcher() const
{
    return d->watermarkWatcher.get();
}

ServerTimeWatcher* SystemContext::serverTimeWatcher() const
{
    return d->serverTimeWatcher.get();
}

nx::vms::rules::Engine* SystemContext::vmsRulesEngine() const
{
    return d->vmsRulesEngineHolder->engine();
}

void SystemContext::setMessageProcessor(QnCommonMessageProcessor* messageProcessor)
{
    base_type::setMessageProcessor(messageProcessor);

    auto clientMessageProcessor = static_cast<QnClientMessageProcessor*>(messageProcessor);
    d->userWatcher->setMessageProcessor(clientMessageProcessor);
    nx::vms::rules::EngineHolder::connectEngine(
        vmsRulesEngine(),
        clientMessageProcessor,
        Qt::QueuedConnection);
}

} // namespace nx::vms::client::desktop
