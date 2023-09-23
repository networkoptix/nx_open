// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_connection.h"

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <nx/network/url/url_builder.h>
#include <nx/p2p/p2p_message_bus.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/ec2/base_ec2_connection.h>
#include <nx_ec/abstract_ec_connection_factory.h>
#include <transaction/json_transaction_serializer.h>
#include <transaction/threadsafe_message_bus_adapter.h>
#include <transaction/ubjson_transaction_serializer.h>

#include "certificate_verifier.h"
#include "query_processor.h"
#include "time_sync_manager.h"

using namespace ec2;

namespace nx::vms::client::core {

/**
 * Actual system connection, provides access to different manager classes, which in their turn
 * represent an abstraction layer to send and receive transactions. Managers are using query
 * processor adaptor via this class interface to send actual network requests.
 */
class MessageBusConnection:
    public BaseEc2Connection<QueryProcessor>,
    public SystemContextAware
{
    using base_type = BaseEc2Connection<QueryProcessor>;

public:
    MessageBusConnection(
        nx::vms::api::PeerType peerType,
        nx::vms::api::ModuleInformation moduleInformation,
        QueryProcessorPtr queryProcessor,
        TransactionMessageBusAdapter* messageBus,
        nx::vms::common::AbstractTimeSyncManagerPtr timeSyncManager)
        :
        base_type(queryProcessor.get()),
        SystemContextAware(messageBus->systemContext()),
        m_peerType(peerType),
        m_moduleInformation(moduleInformation),
        m_messageBus(messageBus),
        m_timeSyncManager(timeSyncManager)
    {
    }

    virtual void startReceivingNotifications() override
    {
        const nx::network::SocketAddress address = queryProcessor()->address();

        const nx::utils::Url url = nx::network::url::Builder()
            .setScheme(nx::network::http::kSecureUrlSchemeName)
            .setEndpoint(address)
            .toUrl();

        m_messageBus->init<nx::p2p::MessageBus>(m_peerType, systemContext());
        m_messageBus->setHandler(notificationManager());

        NX_VERBOSE(this, "Start receiving notifications from %1 (%2)",
            m_moduleInformation.id,
            url);

        base_type::startReceivingNotifications();

        m_messageBus->addOutgoingConnectionToPeer(m_moduleInformation.id,
            nx::vms::api::PeerType::server,
            url,
            queryProcessor()->credentials(),
            queryProcessor()->adapterFunc(m_moduleInformation.id));
    }

    virtual void stopReceivingNotifications() override
    {
        NX_VERBOSE(this, "Stop receiving notifications from %1", m_moduleInformation.id);
        base_type::stopReceivingNotifications();
        if (NX_ASSERT(m_messageBus))
        {
            m_messageBus->removeOutgoingConnectionFromPeer(m_moduleInformation.id);
            m_messageBus->removeHandler(notificationManager());
            m_messageBus->reset();
        }
    }

    // TODO: #sivanov Remove these methods from the abstract class.
    virtual nx::vms::api::Timestamp getTransactionLogTime() const override
    {
        NX_ASSERT(false, "Not implemented");
        return {};
    }

    virtual std::set<QnUuid> getRemovedObjects() const override
    {
        NX_ASSERT(false, "Not implemented");
        return std::set<QnUuid>();
    }

    virtual bool resyncTransactionLog(
        const std::set<QnUuid>& /*filter*/) override
    {
        NX_ASSERT(false, "Not implemented");
        return false;
    }

    virtual void setTransactionLogTime(nx::vms::api::Timestamp /*value*/) override
    {
        NX_ASSERT(false, "Not implemented");
    }

    virtual nx::vms::api::ModuleInformation moduleInformation() const override
    {
        return m_moduleInformation;
    }

    /** Address of the server we are currently connected to. */
    virtual nx::network::SocketAddress address() const override
    {
        return queryProcessor()->address();
    }

    /** Credentials we are using to authorize the connection. */
    virtual nx::network::http::Credentials credentials() const override
    {
        return queryProcessor()->credentials();
    }

    virtual TransactionMessageBusAdapter* messageBus() const override
    {
        return m_messageBus;
    }

    virtual nx::vms::common::AbstractTimeSyncManagerPtr timeSyncManager() const override
    {
        return m_timeSyncManager;
    }

private:
    const nx::vms::api::PeerType m_peerType;
    const nx::vms::api::ModuleInformation m_moduleInformation;
    TransactionMessageBusAdapter* m_messageBus;
    nx::vms::common::AbstractTimeSyncManagerPtr m_timeSyncManager;
};

struct RemoteConnection::Private
{
    const nx::vms::api::PeerType peerType;
    const nx::vms::api::ModuleInformation moduleInformation;
    ConnectionInfo connectionInfo;
    std::optional<std::chrono::microseconds> sessionTokenExpirationTime;
    std::shared_ptr<CertificateCache> certificateCache;
    QueryProcessorPtr queryProcessor;
    rest::ServerConnectionPtr serverApi;
    mutable nx::Mutex mutex;

    nx::vms::common::AbstractTimeSyncManagerPtr timeSynchronizationManager;
    std::unique_ptr<QnJsonTransactionSerializer> jsonTranSerializer =
        std::make_unique<QnJsonTransactionSerializer>();
    std::unique_ptr<QnUbjsonTransactionSerializer> ubjsonTranSerializer =
        std::make_unique<QnUbjsonTransactionSerializer>();
    std::unique_ptr<ThreadsafeMessageBusAdapter> messageBus;
    std::shared_ptr<MessageBusConnection> messageBusConnection;

    Private(
        nx::vms::api::PeerType peerType,
        const nx::vms::api::ModuleInformation& moduleInformation,
        ConnectionInfo connectionInfo,
        std::optional<std::chrono::microseconds> sessionTokenExpirationTime,
        std::shared_ptr<CertificateCache> certificateCache,
        Qn::SerializationFormat serializationFormat,
        QnUuid sessionId)
        :
        peerType(peerType),
        moduleInformation(moduleInformation),
        connectionInfo(std::move(connectionInfo)),
        sessionTokenExpirationTime(sessionTokenExpirationTime),
        certificateCache(certificateCache)
    {
        serverApi = std::make_shared<rest::ServerConnection>(
            moduleInformation.id,
            sessionId,
            certificateCache.get(),
            this->connectionInfo.address,
            this->connectionInfo.credentials);

        queryProcessor = std::make_shared<QueryProcessor>(
            moduleInformation.id,
            sessionId,
            certificateCache.get(),
            this->connectionInfo.address,
            this->connectionInfo.credentials,
            serializationFormat);
    }
};

RemoteConnection::RemoteConnection(
    nx::vms::api::PeerType peerType,
    const nx::vms::api::ModuleInformation& moduleInformation,
    ConnectionInfo connectionInfo,
    std::optional<std::chrono::microseconds> sessionTokenExpirationTime,
    std::shared_ptr<CertificateCache> certificateCache,
    Qn::SerializationFormat serializationFormat,
    QnUuid sessionId,
    QObject* parent)
    :
    QObject(parent),
    d(new Private(
        peerType,
        moduleInformation,
        std::move(connectionInfo),
        sessionTokenExpirationTime,
        certificateCache,
        serializationFormat,
        sessionId))
{
}

void RemoteConnection::updateSessionId(const QnUuid& sessionId)
{
    d->serverApi->updateSessionId(sessionId);
    d->queryProcessor->updateSessionId(sessionId);
}

RemoteConnection::~RemoteConnection()
{
}

void RemoteConnection::initializeMessageBusConnection(
    QnCommonModule* commonModule)
{
    if (!NX_ASSERT(!d->messageBus, "Connection is already initialized"))
        return;

    // FIXME: #sivanov Make Remote Connection - System Context Aware.
    auto systemContext = dynamic_cast<SystemContext*>(commonModule->systemContext());

    d->timeSynchronizationManager =
        std::make_shared<TimeSyncManager>(systemContext, d->moduleInformation.id);
    d->messageBus = std::make_unique<ThreadsafeMessageBusAdapter>(
        systemContext,
        d->jsonTranSerializer.get(),
        d->ubjsonTranSerializer.get());
    d->messageBusConnection = std::make_shared<MessageBusConnection>(
        d->peerType,
        d->moduleInformation,
        d->queryProcessor,
        d->messageBus.get(),
        d->timeSynchronizationManager);
    d->messageBusConnection->init(appContext()->moduleDiscoveryManager());
}

const nx::vms::api::ModuleInformation& RemoteConnection::moduleInformation() const
{
    return d->moduleInformation;
}

ConnectionInfo RemoteConnection::connectionInfo() const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->connectionInfo;
}

LogonData RemoteConnection::createLogonData() const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    LogonData logonData{
        .address = d->connectionInfo.address,
        .credentials = d->connectionInfo.credentials,
        .userType = d->connectionInfo.userType,
        .expectedServerId = d->moduleInformation.id,
        .expectedServerVersion = d->moduleInformation.version,
        .expectedCloudSystemId = d->moduleInformation.cloudSystemId,
    };
    return logonData;
}

nx::network::SocketAddress RemoteConnection::address() const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->connectionInfo.address;
}

void RemoteConnection::updateAddress(nx::network::SocketAddress address)
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    d->connectionInfo.address = address;
    d->queryProcessor->updateAddress(address);
    d->serverApi->updateAddress(address);
}

nx::vms::api::UserType RemoteConnection::userType() const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->connectionInfo.userType;
}

nx::network::http::Credentials RemoteConnection::credentials() const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->connectionInfo.credentials;
}

void RemoteConnection::updateCredentials(
    nx::network::http::Credentials credentials,
    std::optional<std::chrono::microseconds> sessionTokenExpirationTime)
{
    {
        NX_MUTEX_LOCKER lk(&d->mutex);
        d->connectionInfo.credentials = credentials;
        d->queryProcessor->updateCredentials(credentials);
        d->serverApi->updateCredentials(credentials);
        d->messageBus->updateOutgoingConnection(d->moduleInformation.id, credentials);
        d->sessionTokenExpirationTime = sessionTokenExpirationTime;
    }
    emit credentialsChanged();
}

std::optional<std::chrono::microseconds> RemoteConnection::sessionTokenExpirationTime() const
{
    return d->sessionTokenExpirationTime;
}

void RemoteConnection::setSessionTokenExpirationTime(std::optional<std::chrono::microseconds> time)
{
    d->sessionTokenExpirationTime = time;
}

rest::ServerConnectionPtr RemoteConnection::serverApi() const
{
    return d->serverApi;
}

AbstractECConnectionPtr RemoteConnection::messageBusConnection() const
{
    return d->messageBusConnection;
}

common::AbstractTimeSyncManagerPtr RemoteConnection::timeSynchronizationManager() const
{
    return d->timeSynchronizationManager;
}

std::shared_ptr<CertificateCache> RemoteConnection::certificateCache() const
{
    return d->certificateCache;
}

} // namespace nx::vms::client::core
