// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context.h"

#include <QtQml/QtQml>

#include <client/client_message_processor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/core/watchers/watermark_watcher.h>

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
    std::unique_ptr<UserWatcher> userWatcher;
    std::unique_ptr<WatermarkWatcher> watermarkWatcher;
    std::unique_ptr<ServerTimeWatcher> serverTimeWatcher;

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
    d->userWatcher = std::make_unique<UserWatcher>(this);
    d->watermarkWatcher = std::make_unique<WatermarkWatcher>(this);
    d->serverTimeWatcher = std::make_unique<ServerTimeWatcher>(this);
}

SystemContext::~SystemContext()
{
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

void SystemContext::setMessageProcessor(QnCommonMessageProcessor* messageProcessor)
{
    base_type::setMessageProcessor(messageProcessor);

    auto clientMessageProcessor = static_cast<QnClientMessageProcessor*>(messageProcessor);
    d->userWatcher->setMessageProcessor(clientMessageProcessor);
}

} // namespace nx::vms::client::desktop
