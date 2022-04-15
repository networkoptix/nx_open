// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context.h"

#include <QtQml/QtQml>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>

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
        const auto qmlContext = QQmlEngine::contextForObject(m_owner);
        if (!NX_ASSERT(qmlContext))
            return nullptr;

        return qmlContext->contextProperty(kSystemContextPropertyName).value<SystemContext*>();
    }

private:
    QObject* const m_owner;
};

struct SystemContext::Private
{
    std::unique_ptr<ServerTimeWatcher> serverTimeWatcher;

    mutable nx::Mutex sessionMutex;
    std::shared_ptr<RemoteSession> session;
};

SystemContext::SystemContext(QnUuid peerId, QObject* parent):
    nx::vms::common::SystemContext(
        std::move(peerId),
        /*sessionId*/ QnUuid::createUuid(),
        nx::core::access::Mode::cached,
        parent),
    d(new Private())
{
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

std::shared_ptr<RemoteSession> SystemContext::session() const
{
    NX_MUTEX_LOCKER lock(&d->sessionMutex);
    return d->session;
}

void SystemContext::setSession(std::shared_ptr<RemoteSession> session)
{
    {
        NX_MUTEX_LOCKER lock(&d->sessionMutex);
        d->session = session;
    }
    emit remoteIdChanged(currentServerId());
}

QnUuid SystemContext::currentServerId() const
{
    if (auto connection = this->connection())
        return connection->moduleInformation().id;

    return QnUuid();
}

QnMediaServerResourcePtr SystemContext::currentServer() const
{
    return resourcePool()->getResourceById<QnMediaServerResource>(currentServerId());
}

RemoteConnectionPtr SystemContext::connection() const
{
    NX_MUTEX_LOCKER lock(&d->sessionMutex);
    if (d->session)
        return d->session->connection();

    return {};
}

rest::ServerConnectionPtr SystemContext::connectedServerApi() const
{
    if (auto connection = this->connection())
        return connection->serverApi();

    return {};
}

ServerTimeWatcher* SystemContext::serverTimeWatcher() const
{
    return d->serverTimeWatcher.get();
}

} // namespace nx::vms::client::desktop
