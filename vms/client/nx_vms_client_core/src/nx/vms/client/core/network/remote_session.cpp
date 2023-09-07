// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_session.h"

#include <chrono>

#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtCore/QThread>

#include <api/server_rest_connection.h>
#include <client/client_message_processor.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <nx/reflect/json.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/certificate_verifier.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/credentials_manager.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/utils/cloud_session_token_updater.h>
#include <nx/vms/client/core/utils/reconnect_helper.h>
#include <transaction/message_bus_adapter.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

#include "network_module.h"
#include "remote_connection.h"
#include "remote_connection_factory.h"
#include "session_token_terminator.h"

using namespace std::chrono;

namespace {

static constexpr auto kReconnectDelay = 3s;
static constexpr auto kTokenExpirationUpdateInterval = 1min;

} // namespace

namespace nx::vms::client::core {

struct RemoteSession::Private
{
    QPointer<RemoteConnectionFactory> remoteConnectionFactory;
    QPointer<QnClientMessageProcessor> messageProcessor;
    RemoteConnectionPtr connection;
    State state = State::waitingPeer;
    bool stickyReconnect = false;
    std::unique_ptr<ReconnectHelper> reconnectHelper;

    /** Actual error code for the server we were connected to. */
    std::optional<RemoteConnectionErrorCode> activeServerReconnectErrorCode;

    std::unique_ptr<CloudSessionTokenUpdater> cloudTokenUpdater;
    RemoteConnectionFactory::ProcessPtr currentConnectionProcess;

    mutable nx::Mutex mutex;
    bool autoTerminate = false;

    void terminateServerSessionIfNeeded();
    void updateTokenExpirationTime();
};

void RemoteSession::Private::terminateServerSessionIfNeeded()
{
    if (autoTerminate)
        qnClientCoreModule->sessionTokenTerminator()->terminateToken(connection);
}

void RemoteSession::Private::updateTokenExpirationTime()
{
    connection->serverApi()->getRawResult(
        "/rest/v1/login/sessions/current",
        nx::network::rest::Params(),
        nx::utils::guarded(connection.get(),
            [connection = connection.get()](
                bool success,
                rest::Handle /*requestId*/,
                QByteArray data,
                nx::network::http::HttpHeaders)
            {
                if (!success)
                {
                    NX_WARNING(typeid(RemoteSession), "Can not update token expiration time");
                    return;
                }

                const auto [session, result] =
                    nx::reflect::json::deserialize<nx::vms::api::LoginSession>(data.toStdString());

                if (!NX_ASSERT(result,
                        nx::format("Can not deserialize token expiration time: %1", data)))
                {
                    return;
                }

                connection->setSessionTokenExpirationTime(
                    qnSyncTime->currentTimePoint() + session.expiresInS);
            }),
        QThread::currentThread());
}

RemoteSession::RemoteSession(
    RemoteConnectionPtr connection,
    SystemContext* systemContext,
    QObject* parent)
    :
    base_type(parent),
    SystemContextAware(systemContext),
    d(new Private())
{
    // Audit id should be updated when establishing new session.
    // TODO: #sivanov Remove global singleton storage, use session id as audit id everywhere.
    systemContext->updateRunningInstanceGuid();

    d->remoteConnectionFactory = qnClientCoreModule->networkModule()->connectionFactory();

    d->messageProcessor = static_cast<QnClientMessageProcessor*>(
        systemContext->messageProcessor());

    connect(d->messageProcessor.data(), &QnCommonMessageProcessor::connectionOpened, this,
        &RemoteSession::onMessageBusConnectionOpened);
    connect(d->messageProcessor.data(), &QnCommonMessageProcessor::initialResourcesReceived, this,
        &RemoteSession::onInitialResourcesReceived);
    connect(d->messageProcessor.data(), &QnCommonMessageProcessor::connectionClosed, this,
        &RemoteSession::onMessageBusConnectionClosed);

    establishConnection(connection);

    auto tokenExpirationTimer = new QTimer(this);
    tokenExpirationTimer->setInterval(kTokenExpirationUpdateInterval);
    tokenExpirationTimer->callOnTimeout([this] { d->updateTokenExpirationTime(); });
    tokenExpirationTimer->start();
}

RemoteSession::~RemoteSession()
{
    NX_VERBOSE(this, "Shutdown current session");

    if (d->messageProcessor)
        d->messageProcessor->disconnect(this);

    stopReconnecting();
    d->terminateServerSessionIfNeeded();
    if (d->messageProcessor)
        d->messageProcessor->init({});
    d->cloudTokenUpdater.reset();
    qnSyncTime->setTimeSyncManager({});

    NX_MUTEX_LOCKER lock(&d->mutex);
    d->connection.reset();
}

void RemoteSession::updatePassword(const QString& newPassword)
{
    using HoldConnectionPolicy = QnClientMessageProcessor::HoldConnectionPolicy;
    if (!NX_ASSERT(d->remoteConnectionFactory))
        return;

    NX_ASSERT(QThread::currentThread() == qApp->thread());
    if (NX_ASSERT(d->messageProcessor))
        d->messageProcessor->holdConnection(HoldConnectionPolicy::update);
    auto credentials = d->connection->credentials();

    if (!newPassword.isEmpty())
    {
        credentials.authToken = nx::network::http::PasswordAuthToken(newPassword.toStdString());
        d->connection->updateCredentials(credentials, d->connection->sessionTokenExpirationTime());
    }

    auto callback = nx::utils::guarded(this,
        [this, credentials](RemoteConnectionFactory::ConnectionOrError result)
        {
            if (const auto error = std::get_if<RemoteConnectionError>(&result))
            {
                d->connection->updateCredentials(credentials);
                emit reconnectRequired();
            }
            else
            {
                auto connection = std::get<RemoteConnectionPtr>(result);
                const auto actualCredentials = connection->credentials();
                d->connection->updateCredentials(
                    actualCredentials, connection->sessionTokenExpirationTime());
                stopReconnecting();
                if (d->connection->userType() != nx::vms::api::UserType::cloud)
                {
                    auto systemId = d->connection->moduleInformation().localSystemId;
                    const auto savedCredentials = CredentialsManager::credentials(
                        systemId, actualCredentials.username);
                    const bool passwordIsAlreadySaved =
                        savedCredentials && !savedCredentials->authToken.empty();
                    if (passwordIsAlreadySaved)
                        CredentialsManager::storeCredentials(systemId, actualCredentials);
                }
            }
            if (NX_ASSERT(d->messageProcessor))
                d->messageProcessor->holdConnection(HoldConnectionPolicy::none);
        });

    LogonData logonData = d->connection->createLogonData();
    logonData.credentials = credentials;
    logonData.userInteractionAllowed = false; //< We do not expect any errors here.
    d->currentConnectionProcess = d->remoteConnectionFactory->connect(logonData, callback);
}

void RemoteSession::updateBearerToken(std::string token)
{
    auto credentials = d->connection->credentials();
    credentials.authToken = nx::network::http::BearerAuthToken(token);
    d->connection->updateCredentials(credentials);
    d->updateTokenExpirationTime();
}

void RemoteSession::setStickyReconnect(bool value)
{
    d->stickyReconnect = value;
}

RemoteSession::State RemoteSession::state() const
{
    return d->state;
}

RemoteConnectionPtr RemoteSession::connection() const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    return d->connection;
}

void RemoteSession::setAutoTerminate(bool value)
{
    NX_VERBOSE(this, "Set session auto-terminate to %1", value);
    d->autoTerminate = value;
}

bool RemoteSession::keepCurrentServerOnError(RemoteConnectionErrorCode error)
{
    // In case of network errors just continue to reconnect.
    switch (error)
    {
        case RemoteConnectionErrorCode::connectionTimeout:
        case RemoteConnectionErrorCode::genericNetworkError:
        case RemoteConnectionErrorCode::internalError:
        case RemoteConnectionErrorCode::networkContentError:
            return true;
    }
    return false;
}

void RemoteSession::establishConnection(RemoteConnectionPtr connection)
{
    NX_VERBOSE(this, "Initialize new connection");
    stopReconnecting();

    if (auto oldConnection = this->connection())
    {
        oldConnection->messageBusConnection()->messageBus()->disconnect(this);
        if (NX_ASSERT(d->messageProcessor))
            d->messageProcessor->init({});
        d->cloudTokenUpdater.reset();
        qnSyncTime->setTimeSyncManager({});
    }

    {
        NX_MUTEX_LOCKER lock(&d->mutex);
        if (d->connection)
            d->connection->disconnect(this);

        d->connection = connection;
    }

    connect(d->connection.get(),
        &RemoteConnection::credentialsChanged,
        this,
        &RemoteSession::credentialsChanged);

    // Initialize cloud token updater.
    if (connection->userType() == nx::vms::api::UserType::cloud
        && connection->credentials().authToken.isBearerToken())
    {
        d->cloudTokenUpdater = std::make_unique<CloudSessionTokenUpdater>(this);
        connect(
            d->cloudTokenUpdater.get(),
            &CloudSessionTokenUpdater::sessionTokenExpiring,
            this,
            &RemoteSession::onCloudSessionTokenExpiring);
        d->cloudTokenUpdater->onTokenUpdated(
            connection->sessionTokenExpirationTime().value_or(std::chrono::microseconds::zero()));
    }

    // TODO: #sivanov Remove global singleton storage, use session id as audit id everywhere.
    // Setup audit id.
    connection->updateSessionId(systemContext()->sessionId());

    // Setup message bus connection.
    connection->initializeMessageBusConnection(qnClientCoreModule->commonModule());
    qnSyncTime->setTimeSyncManager(connection->timeSynchronizationManager());
    if (NX_ASSERT(d->messageProcessor))
        d->messageProcessor->init(connection->messageBusConnection());

    auto messageBus = connection->messageBusConnection()->messageBus();
    auto makeServerMarkingFunction=
        [this](RemoteConnectionErrorCode errorCode)
        {
            return
                [this, errorCode]
                {
                    NX_DEBUG(this, "Try to mark server invalid, error: %1, keep: %2",
                        errorCode, keepCurrentServerOnError(errorCode));

                    d->activeServerReconnectErrorCode = errorCode;
                    if (d->reconnectHelper && !keepCurrentServerOnError(errorCode))
                    {
                        d->reconnectHelper->markCurrentServerAsInvalid();
                        checkIfReconnectFailed();
                    }
                };
        };

    // Usually this occurs when client reconnects earlier than server manages to note the client
    // was ever disconnected. Server will cleanup local peer info in a minute, so try again.
    connect(messageBus,
        &ec2::TransactionMessageBusAdapter::remotePeerForbidden,
        this,
        makeServerMarkingFunction(RemoteConnectionErrorCode::genericNetworkError));

    connect(messageBus,
        &ec2::TransactionMessageBusAdapter::remotePeerUnauthorized,
        this,
        [this, makeServerMarkingFunction]
        {
            auto errorCode = RemoteConnectionErrorCode::unauthorized;
            auto connection = this->connection();
            if (connection->credentials().authToken.isBearerToken())
            {
                if (connection->connectionInfo().isCloud())
                    errorCode = RemoteConnectionErrorCode::cloudSessionExpired;
                else if (connection->connectionInfo().isTemporary())
                    errorCode = RemoteConnectionErrorCode::temporaryTokenExpired;
                else
                    errorCode = RemoteConnectionErrorCode::sessionExpired;
            }
            makeServerMarkingFunction(errorCode)();
        });

    connect(messageBus,
        &ec2::TransactionMessageBusAdapter::remotePeerHandshakeError,
        this,
        makeServerMarkingFunction(RemoteConnectionErrorCode::certificateRejected));

    setState(State::waitingPeer);
}

void RemoteSession::setState(State value)
{
    if (d->state == value)
        return;

    NX_DEBUG(this, "Session state change: %1 -> %2", d->state, value);
    d->state = value;
    emit stateChanged(value);
}

void RemoteSession::onMessageBusConnectionOpened()
{
    NX_VERBOSE(this, "Connection opened");
    stopReconnecting();
    setState(State::waitingResources);
}

void RemoteSession::onInitialResourcesReceived()
{
    NX_VERBOSE(this, "Resources received");
    setState(State::connected);
}

void RemoteSession::onMessageBusConnectionClosed()
{
    NX_DEBUG(this, "Connection closed, trying to reconnect");
    tryToRestoreConnection();
}

void RemoteSession::tryToRestoreConnection()
{
    if (d->reconnectHelper)
        return;

    d->reconnectHelper = std::make_unique<ReconnectHelper>(systemContext(), d->stickyReconnect);

    // Message bus may disconnect before full info processing.
    if (d->reconnectHelper->empty() && state() != State::connected)
    {
        d->activeServerReconnectErrorCode = RemoteConnectionErrorCode::genericNetworkError;
        NX_ASSERT(checkIfReconnectFailed(), "State: %1", state());

        return;
    }

    setState(State::reconnecting);
    reconnectStep();
}

void RemoteSession::stopReconnecting()
{
    d->reconnectHelper.reset();
    d->currentConnectionProcess.reset();
}

void RemoteSession::reconnectStep()
{
    // Connection can already be restored or broken.
    if (!d->reconnectHelper)
        return;

    if (!NX_ASSERT(!checkIfReconnectFailed(), "Workflow error"))
        return;

    if (!NX_ASSERT(d->remoteConnectionFactory))
        return;

    d->reconnectHelper->next();
    emit reconnectingToServer(d->reconnectHelper->currentServer());

    auto reconnectRequestCallback = nx::utils::guarded(this,
        [this](RemoteConnectionFactory::ConnectionOrError result)
        {
            d->currentConnectionProcess.reset();
            if (const auto error = std::get_if<RemoteConnectionError>(&result))
            {
                switch (error->code)
                {
                    // Database was cleaned up during restart, e.g. merge to other system.
                    case RemoteConnectionErrorCode::factoryServer:
                    case RemoteConnectionErrorCode::unauthorized:

                    // Server was updated.
                    case RemoteConnectionErrorCode::customizationDiffers:
                    case RemoteConnectionErrorCode::cloudHostDiffers:
                    case RemoteConnectionErrorCode::versionIsTooLow:
                    case RemoteConnectionErrorCode::binaryProtocolVersionDiffers:
                    case RemoteConnectionErrorCode::certificateRejected:
                        d->reconnectHelper->markCurrentServerAsInvalid();
                        checkIfReconnectFailed();
                        break;

                    // In all other cases we can try again later.
                    default:
                        break;
                }
                executeDelayedParented(
                    [this] { reconnectStep(); }, this);
            }
            else
            {
                establishConnection(std::get<RemoteConnectionPtr>(result));
            }
        });

    // Reconnect to the current server occurs automatically in the message bus, so we don't need to
    // send additional request. Reconnect helper returns empty address in this case.
    if (auto address = d->reconnectHelper->currentAddress())
    {
        LogonData logonData{
            .address = *address,
            .credentials = d->connection->credentials(),
            .userType = d->connection->userType(),
            .expectedServerId = d->reconnectHelper->currentServer()->getId(),
            .expectedServerVersion = d->connection->moduleInformation().version,
            .expectedCloudSystemId = d->connection->moduleInformation().cloudSystemId,
            .userInteractionAllowed = false
        };

        NX_DEBUG(this, "Performing reconnect attempt to: %1", address);
        d->currentConnectionProcess = d->remoteConnectionFactory->connect(
            logonData,
            reconnectRequestCallback);
    }
    else
    {
        NX_DEBUG(this, "Scheduling reconnect attempt for: %1", kReconnectDelay);

        if (d->activeServerReconnectErrorCode == RemoteConnectionErrorCode::cloudSessionExpired
            && NX_ASSERT(d->cloudTokenUpdater))
        {
            d->cloudTokenUpdater->updateTokenIfNeeded();
        }

        executeDelayedParented(
            [this] { reconnectStep(); }, kReconnectDelay, this);
    }
}

bool RemoteSession::checkIfReconnectFailed()
{
    bool result = d->reconnectHelper && d->reconnectHelper->empty();
    if (result)
    {
        NX_ASSERT(d->activeServerReconnectErrorCode, "Current server connection must be broken");
        emit reconnectFailed(
            d->activeServerReconnectErrorCode.value_or(RemoteConnectionErrorCode::internalError));
    }
    return result;
}

void RemoteSession::onCloudSessionTokenExpiring()
{
    using namespace nx::cloud::db::api;

    auto handler =
        [this](ResultCode result, IssueTokenResponse response)
        {
            NX_DEBUG(this, "Reissue token result: %1", result);
            auto connection = this->connection();
            if (result == ResultCode::ok && connection)
            {
                auto credentials = nx::network::http::Credentials(
                    connection->credentials().username,
                    nx::network::http::BearerAuthToken(response.access_token));
                connection->updateCredentials(std::move(credentials), response.expires_at);
                d->cloudTokenUpdater->onTokenUpdated(response.expires_at);
            }
        };

    auto connection = this->connection();
    if (!connection || !connection->connectionInfo().isCloud())
        return;

    IssueTokenRequest request;
    request.grant_type = GrantType::refresh_token;
    request.scope =
        nx::format("cloudSystemId=%1", connection->moduleInformation().cloudSystemId).toStdString();
    request.refresh_token = qnCloudStatusWatcher->remoteConnectionCredentials().authToken.value;

    NX_DEBUG(this, "Reissueing cloud access token");
    d->cloudTokenUpdater->issueToken(request, std::move(handler), this);
}

} // namespace nx::vms::client::core
