// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context.h"

#include <QtQml/QtQml>

#include <client/client_message_processor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/cross_system/cross_system_ptz_controller_pool.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/network/server_primary_interface_watcher.h>
#include <nx/vms/client/core/ptz/client_ptz_controller_pool.h>
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
    std::unique_ptr<QnPtzControllerPool> ptzControllerPool;
    std::unique_ptr<UserWatcher> userWatcher;
    std::unique_ptr<WatermarkWatcher> watermarkWatcher;
    std::unique_ptr<ServerTimeWatcher> serverTimeWatcher;
    std::unique_ptr<ServerPrimaryInterfaceWatcher> serverPrimaryInterfaceWatcher;
    std::unique_ptr<nx::vms::rules::EngineHolder> vmsRulesEngineHolder;
    std::unique_ptr<AccessController> accessController;

    mutable nx::Mutex sessionMutex;

    /** Session this context belongs to. Exclusive with connection. */
    std::shared_ptr<RemoteSession> session;

    /** Connection this context belongs to. Exclusive with session. */
    RemoteConnectionPtr connection;
};

SystemContext::SystemContext(
    Mode mode,
    QnUuid peerId,
    nx::core::access::Mode resourceAccessMode,
    QObject* parent)
    :
    base_type(
        mode,
        std::move(peerId),
        /*sessionId*/ QnUuid::createUuid(),
        resourceAccessMode,
        parent),
    d(new Private())
{
    d->serverTimeWatcher = std::make_unique<ServerTimeWatcher>(this);

    switch (mode)
    {
        case Mode::client:
            d->ptzControllerPool = std::make_unique<ptz::ControllerPool>(this);
            d->userWatcher = std::make_unique<UserWatcher>(this);
            d->watermarkWatcher = std::make_unique<WatermarkWatcher>(this);
            d->serverPrimaryInterfaceWatcher = std::make_unique<ServerPrimaryInterfaceWatcher>(
                this);
            d->vmsRulesEngineHolder = std::make_unique<nx::vms::rules::EngineHolder>(
                this,
                std::make_unique<nx::vms::rules::Initializer>(this),
                /*separateThread*/ false);
            break;

        case Mode::crossSystem:
            d->ptzControllerPool = std::make_unique<CrossSystemPtzControllerPool>(this);
            d->userWatcher = std::make_unique<UserWatcher>(this);
            d->watermarkWatcher = std::make_unique<WatermarkWatcher>(this);
            d->serverPrimaryInterfaceWatcher = std::make_unique<ServerPrimaryInterfaceWatcher>(
                this);
            break;

        case Mode::cloudLayouts:
            break;

        case Mode::unitTests:
            break;
    }

    resetAccessController(new AccessController(this));

    if (d->userWatcher)
    {
        connect(d->userWatcher.get(), &UserWatcher::userChanged, this,
            [this](const QnUserResourcePtr& user)
            {
                if (d->accessController)
                    d->accessController->setUser(user);
            });
    }
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
        // Make sure existing session will be terminated outside of the mutex.
        std::swap(d->session, session);
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

nx::network::http::Credentials SystemContext::connectionCredentials() const
{
    if (auto connection = this->connection())
        return connection->credentials();

    return {};
}

rest::ServerConnectionPtr SystemContext::connectedServerApi() const
{
    if (auto connection = this->connection())
        return connection->serverApi();

    return {};
}

ec2::AbstractECConnectionPtr SystemContext::messageBusConnection() const
{
    if (auto connection = this->connection())
        return connection->messageBusConnection();

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

common::SessionTokenHelperPtr SystemContext::getSessionTokenHelper() const
{
    return nullptr;
}

void SystemContext::setMessageProcessor(QnCommonMessageProcessor* messageProcessor)
{
    base_type::setMessageProcessor(messageProcessor);

    auto clientMessageProcessor = qobject_cast<QnClientMessageProcessor*>(messageProcessor);
    if (!NX_ASSERT(clientMessageProcessor, "Invalid message processor type"))
        return;

    if (!vmsRulesEngine())
        return;

    nx::vms::rules::EngineHolder::connectEngine(
        vmsRulesEngine(),
        clientMessageProcessor,
        Qt::QueuedConnection);
}

AccessController* SystemContext::accessController() const
{
    return d->accessController.get();
}

void SystemContext::resetAccessController(AccessController* accessController)
{
    d->accessController.reset(accessController);
}

} // namespace nx::vms::client::desktop
