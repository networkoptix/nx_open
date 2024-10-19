// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "session.h"

#include <chrono>

#include <QtCore/QDebug>
#include <QtCore/QElapsedTimer>
#include <QtGui/QGuiApplication>

#include <client/client_message_processor.h>
#include <client_core/client_core_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <mobile_client/mobile_client_message_processor.h>
#include <mobile_client/mobile_client_module.h>
#include <mobile_client/mobile_client_settings.h>
#include <network/system_helpers.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_common.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/credentials_manager.h>
#include <nx/vms/client/core/network/logon_data_helpers.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_connection_factory.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/client/core/utils/reconnect_helper.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/mobile/push_notification/details/push_ipc_data.h>
#include <nx/vms/client/mobile/ui/qml_wrapper_helper.h>
#include <nx/vms/common/system_settings.h>
#include <utils/common/delayed.h>

using RemoteConnectionErrorCode = nx::vms::client::core::RemoteConnectionErrorCode;

namespace nx::vms::client::mobile {

namespace {

using namespace std::chrono;

/**
 * Detects if error is fatal and there is no sense trying to reconect to the system.
 */
bool isFatalError(RemoteConnectionErrorCode result)
{
    switch (result)
    {
        case RemoteConnectionErrorCode::genericNetworkError:
        case RemoteConnectionErrorCode::forbiddenRequest:
        case RemoteConnectionErrorCode::ldapInitializationInProgress:
        case RemoteConnectionErrorCode::connectionTimeout:
            return false;
        default:
            return true;
    }
}

bool isDigestCloudAuthNeeded(const core::SystemDescriptionPtr& system,
    const std::optional<nx::vms::client::core::LogonData>& logonData)
{
    if (!system || !logonData)
        return true;

    return !system->isOauthSupported() && !logonData->credentials.authToken.isPassword();
};

bool hasStoredToken(const nx::Uuid& systemId, const std::string& userName)
{
    const auto storedCredentials = nx::vms::client::core::CredentialsManager::credentials(
        systemId, userName);

    return storedCredentials && !storedCredentials->authToken.empty();
}

bool hasBearerToken(const nx::Uuid& systemId, const std::string& userName)
{
    const auto storedCredentials = nx::vms::client::core::CredentialsManager::credentials(
        systemId, userName);

    return storedCredentials && storedCredentials->authToken.isBearerToken();
}

void saveLastUsedConnection(
    const nx::vms::api::ModuleInformation& moduleInfo,
    const nx::vms::client::core::LogonData& logonData)
{
    using namespace nx::vms::client::core;

    const auto userName = logonData.credentials.username;
    const bool isCloudConnection = logonData.userType == nx::vms::api::UserType::cloud;
    const auto url =
        [isCloudConnection, address = logonData.address, userName]()
        {
            nx::network::url::Builder builder;
            builder.setEndpoint(address);
            builder.setUserName(userName);

            builder.setScheme(isCloudConnection
                ? QString::fromStdString(std::string(Session::kCloudUrlScheme))
                : nx::network::http::kSecureUrlSchemeName);

            return builder.toUrl();
        }();

    const auto systemId = isCloudConnection
        ? nx::Uuid(::helpers::getTargetSystemId(moduleInfo))
        : ::helpers::getLocalSystemId(moduleInfo);

    if (!isCloudConnection)
    {
        auto credentials = logonData.credentials;
        if (!qnSettings->savePasswords() && !hasStoredToken(systemId, userName))
        {
            // Do not clear credentials if we still have it.
            credentials.authToken = {};
        }

        nx::vms::client::core::CredentialsManager::storeCredentials(systemId, credentials);

        appContext()->coreSettings()->storeRecentConnection(
            systemId,
            moduleInfo.systemName,
            moduleInfo.version,
            url);
    }
    else if (logonData.credentials.authToken.isPassword()
        && logonData.credentials.username == qnCloudStatusWatcher->cloudLogin().toStdString())
    {
        // To avoid password entering each time we want to connect to the old system we
        // store digest password.
        appContext()->coreSettings()->digestCloudPassword =
            logonData.credentials.authToken.value;
    }

    appContext()->coreSettings()->lastConnection =
        [isCloudConnection, url, systemId, userName]()
        {
            return isCloudConnection || hasStoredToken(systemId, userName)
                ? nx::vms::client::core::ConnectionData{url, systemId}
                : nx::vms::client::core::ConnectionData();
        }();
}

QnClientMessageProcessor* messageProcessor(nx::vms::client::core::SystemContext* context)
{
    return static_cast<QnClientMessageProcessor*>(context->messageProcessor());
}

using State = nx::vms::client::mobile::Session::ConnectionState;
bool connectionInProgress(State state)
{
    return state == State::connecting || state == State::loading;
}

} // namespace

//-------------------------------------------------------------------------------------------------

class Session::Private: public QObject
{
public:
    Private(
        const ConnectionData& connectionData,
        const QString& supposedSystemName,
        session::ConnectionCallback&& callback,
        Session* q);

    core::LogonData logonData() const;
    QString currentUser() const;
    nx::Uuid localSystemId() const;

    void connectToServer();
    void connectToKnownServer(nx::vms::client::core::RemoteConnectionPtr connection);
    void disconnectFromServer();
    void handleSuccessfullReply();
    void handleErrorReply(RemoteConnectionErrorCode errorCode);

    ConnectionState connectionState() const;
    void updateConnectionState();

    bool wasConnected() const;
    void setWasConnected();

    bool suspended() const;
    void setSuspended(bool value);

    void updateLogonData(const core::LogonData& value);
    void setAddress(
        const nx::network::SocketAddress& address,
        const nx::Uuid& expectedServerId);
    void resetAddress();

    const QString& systemName() const;
    const ConnectionData& connectionData() const;
    const nx::utils::SoftwareVersion& connectionVersion() const;

    void handleFatalErrorOccured(RemoteConnectionErrorCode errorCode);

    void gatherInitialConnectionInfo(int remainingTriesCount = 20);

private:
    void resetCurrentConnection();
    void handleConnectionClosed();
    void callInitialConnectionCallbackOnce(
        std::optional<RemoteConnectionErrorCode> errorCode = std::nullopt);
    void handleApplicationStateChanged(Qt::ApplicationState state);
    void handleInitialResourcesReceived();
    void updateSystemName(const QString& value);
    void setConnectionVersion(const nx::utils::SoftwareVersion& value);

private:
    Session* const q;
    const ConnectionData m_connectionData;
    std::optional<core::LogonData> m_logonData;
    QString m_systemName;
    nx::utils::SoftwareVersion m_connectionVersion;
    QString m_currentUser;
    nx::Uuid m_localSystemId;
    session::ConnectionCallback m_initialConnectionCallback;

    std::shared_ptr<core::ReconnectHelper> m_reconnectHelper;

    ConnectionState m_state = ConnectionState::disconnected;
    bool m_suspended = false;
    bool m_wasConnected = false;
    nx::vms::client::core::RemoteConnectionFactory::ProcessPtr m_connectionProcess;

    QElapsedTimer m_connectionTimer;
    QTimer m_suspendedTimer;
};

Session::Private::Private(
    const ConnectionData& connectionData,
    const QString& supposedSystemName,
    session::ConnectionCallback&& callback,
    Session* q):
    q(q),
    m_connectionData(connectionData),
    m_systemName(supposedSystemName),
    m_initialConnectionCallback(std::move(callback))
{
    const auto processor = mobile::messageProcessor(q->systemContext());
    connect(processor, &QnClientMessageProcessor::connectionClosed,
        this, &Private::handleConnectionClosed);
    connect(processor->connectionStatus(), &QnClientConnectionStatus::stateChanged,
        this, &Private::updateConnectionState);

    connect(processor, &QnMobileClientMessageProcessor::initialResourcesReceived,
        this, &Private::handleInitialResourcesReceived);
    connect(qApp, &QGuiApplication::applicationStateChanged,
        this, &Private::handleApplicationStateChanged);

    QPointer<nx::vms::common::SystemSettings> settings(q->globalSettings());
    connect(settings, &nx::vms::common::SystemSettings::systemNameChanged, this,
        [this, settings]()
        {
            // 2.6 servers use another property name for systemName, so in 3.0 it's always empty.
            // However we fill system name with session start.
            // This value won't change, but it's not so critical.
            updateSystemName(settings->systemName());
        });

    m_suspendedTimer.setInterval(2min);
    m_suspendedTimer.setSingleShot(true);
    connect(&m_suspendedTimer, &QTimer::timeout, this,  [this]() { setSuspended(true); });
}

core::LogonData Session::Private::logonData() const
{
    return m_logonData.value_or(core::LogonData());
}

QString Session::Private::currentUser() const
{
    return m_currentUser;
}

nx::Uuid Session::Private::localSystemId() const
{
    return m_localSystemId;
}

void Session::Private::connectToServer()
{
    if (!m_logonData)
    {
        // Logon data is not ready yet. Connection will be established as soon as it is gathered.
        return;
    }

    resetCurrentConnection();
    if (m_logonData->address.isNull())
    {
        NX_DEBUG(this, "connectToServer(): address is empty");
        disconnectFromServer();
        handleErrorReply(RemoteConnectionErrorCode::genericNetworkError);
        return;
    }

    NX_DEBUG(this, "connectToServer(): starting connection to <%1>",
        m_logonData->address.toString());

    if (!m_connectionTimer.isValid()) //< No connection process was started.
        m_connectionTimer.start();

    auto callback = nx::utils::guarded(this,
        [this](nx::vms::client::core::RemoteConnectionFactory::ConnectionOrError result)
        {
            NX_DEBUG(this, "connectToServer(): callback: received reply");
            const auto version = m_connectionProcess->context->moduleInformation.version;
            m_connectionProcess.reset();

            if (const auto error =
                std::get_if<nx::vms::client::core::RemoteConnectionError>(&result))
            {
                NX_WARNING(this, "Connection failed: %1", error->toString());
                handleErrorReply(error->code);
                return;
            }
            else
            {
                auto connection = std::get<nx::vms::client::core::RemoteConnectionPtr>(result);
                setConnectionVersion(version);
                NX_DEBUG(this, "connectToServer(): callback: end");
                connectToKnownServer(connection);
            }
        });

    const auto factory = qnClientCoreModule->networkModule()->connectionFactory();
    m_connectionProcess = factory->connect(m_logonData.value(), callback, q->systemContext());

    updateConnectionState();
    NX_DEBUG(this, "connectToServer(): end");
}

void Session::Private::connectToKnownServer(nx::vms::client::core::RemoteConnectionPtr connection)
{
    if (!NX_ASSERT(connection))
        return;

    updateLogonData(connection->createLogonData());
    updateConnectionState();

    NX_DEBUG(this, "connectToKnownServer(): start, server id is <%1>",
        connection->moduleInformation().id);

    nx::vms::api::ModuleInformation moduleInformation = connection->moduleInformation();
    core::appContext()->knownServerConnectionsWatcher()->saveConnection(moduleInformation.id,
        connection->address());
    updateSystemName(moduleInformation.systemName);

    const bool restoringConnection = wasConnected();

    const auto session = std::make_shared<core::RemoteSession>(connection, q->systemContext());
    // TODO: #ynikitenkov Here probably should be session signal handlers like reconnect processing.

    qnClientCoreModule->networkModule()->setSession(session);
    q->systemContext()->setSession(session);

    const auto userName = QString::fromStdString(connection->credentials().username);

    const auto localSystemId = helpers::getLocalSystemId(moduleInformation);
    const auto user = q->isCloudSession()
        ? core::appContext()->cloudStatusWatcher()->cloudLogin()
        : userName;

    if (m_localSystemId != localSystemId || m_currentUser != user)
    {
        m_localSystemId = localSystemId;
        m_currentUser = user;
        emit q->parametersChanged(m_localSystemId, m_currentUser);
    }

    handleSuccessfullReply();

    if (restoringConnection)
    {
        NX_DEBUG(this, "connectToKnownServer(): callback: end: successfully restored");
        return;
    }

    m_localSystemId = localSystemId;
    saveLastUsedConnection(moduleInformation, logonData());
    NX_DEBUG(this, "connectToKnownServer(): callback: end: saving credentials, connected");
}

void Session::Private::resetCurrentConnection()
{
    m_connectionProcess.reset();
    q->systemContext()->setSession({});
    qnClientCoreModule->networkModule()->setSession({});

    updateConnectionState();
}

void Session::Private::disconnectFromServer()
{
    NX_DEBUG(this, "disconnectFromServer(): called");
    m_reconnectHelper.reset();
    resetCurrentConnection();
}

void Session::Private::handleSuccessfullReply()
{
    NX_DEBUG(this, "handleSuccessfullReply(): successfully connected");
    updateConnectionState();

    m_connectionTimer.invalidate();
    m_reconnectHelper.reset();
}

void Session::Private::handleErrorReply(RemoteConnectionErrorCode errorCode)
{
    NX_DEBUG(this, "handleErrorReply(): result is <%1>", (int)errorCode);

    updateConnectionState();

    static constexpr milliseconds kInitialConnectionTimeoutMs = 10s;
    static constexpr seconds kRetryDelay(3);

    if (isFatalError(errorCode))
    {
        handleFatalErrorOccured(errorCode);
        return;
    }

    if (m_wasConnected) //< Restoring connection.
    {
        if (!m_reconnectHelper)
            m_reconnectHelper.reset(new core::ReconnectHelper(q->systemContext()));

        m_reconnectHelper->next();
        const auto nextAddress = m_reconnectHelper->currentAddress();
        if (nextAddress)
        {
            NX_DEBUG(this, "handleErrorReply(): trying reconnect with next server <%1>",
                nextAddress.value());
            setAddress(nextAddress.value(), m_reconnectHelper->currentServer()->getId());
        }
        else
        {
            NX_DEBUG(this, "handleErrorReply(): next address is empty, using the old one");
        }

        executeDelayedParented([this]() { connectToServer(); }, kRetryDelay, q);
        return;
    }

    if (m_connectionTimer.isValid() && m_connectionTimer.elapsed() < kInitialConnectionTimeoutMs.count())
    {
        NX_DEBUG(this, "handleErrorReply(): one more try to finish initial connection process");
        executeDelayedParented([this]() { connectToServer(); }, kRetryDelay, q);
        return;
    }

    NX_DEBUG(this, "handleErrorReply(): Can't connect initially");
    handleFatalErrorOccured(errorCode);
}

Session::ConnectionState Session::Private::connectionState() const
{
    return m_state;
}

void Session::Private::updateConnectionState()
{
    ConnectionState targetState = connectionState();

    NX_DEBUG(this, "updateConnectionState(): start: initial state is <%1>", targetState);
    if (suspended())
    {
        targetState = ConnectionState::suspended;

        NX_DEBUG(this, "updateConnectionState(): suspended, state is <%1>", targetState);
    }
    else if (m_connectionProcess)
    {
        targetState = wasConnected()
            ? ConnectionState::reconnecting
            : ConnectionState::connecting;

        NX_DEBUG(this,
            "updateConnectionState(): connection is in process, target state is <%1>",
            targetState);
    }
    else
    {
        targetState = static_cast<ConnectionState>(
            mobile::messageProcessor(q->systemContext())->connectionStatus()->state());

        NX_DEBUG(this, "updateConnectionState(): current processor state is <%1>", targetState);
    }

    if (targetState == connectionState())
    {
        NX_DEBUG(this, "updateConnectionState(): end: state is the same");
        return;
    }

    m_state = targetState;

    NX_DEBUG(this, "updateConnectionState(): end: new state is <%1>", targetState);
    emit q->connectionStateChanged();
}

bool Session::Private::wasConnected() const
{
    return m_wasConnected;
}

void Session::Private::setWasConnected()
{
    if (m_wasConnected)
        return;

    NX_DEBUG(this, "setWasConnected(): called");
    m_wasConnected = true;
    emit q->wasConnectedChanged();
}

bool Session::Private::suspended() const
{
    return m_suspended;
}

void Session::Private::setSuspended(bool value)
{
    if (m_suspended == value)
        return;

    NX_DEBUG(this, "setSuspended(): new value is <%1>", value);
    m_suspended = value;
    updateConnectionState();

    if (m_suspended)
        disconnectFromServer();
    else
        connectToServer();
}

void Session::Private::updateLogonData(const core::LogonData& value)
{
    const auto currentValue = logonData();
    m_logonData = value;
    if (currentValue.address != value.address)
        emit q->addressChanged();
    if (currentValue.userType != value.userType)
        emit q->isCloudSessionChanged();
}

void Session::Private::setAddress(const nx::network::SocketAddress& address,
    const nx::Uuid& expectedServerId)
{
    if (logonData().address == address)
        return;

    NX_DEBUG(this, "setAddress(): new address is <%1>", address.toString());
    if (!NX_ASSERT(m_logonData.has_value(), "Unexpected logon data value state."))
        return;

    m_logonData->address = address;
    m_logonData->expectedServerId = expectedServerId;
    emit q->addressChanged();
}

void Session::Private::resetAddress()
{
    setAddress({}, {});
}

const QString& Session::Private::systemName() const
{
    return m_systemName;
}

const Session::ConnectionData& Session::Private::connectionData() const
{
    return m_connectionData;
}

const nx::utils::SoftwareVersion& Session::Private::connectionVersion() const
{
    return m_connectionVersion;
}

void Session::Private::handleFatalErrorOccured(RemoteConnectionErrorCode errorCode)
{
    NX_DEBUG(this, "handleFatalErrorOccured(): called");

    // Make sure fatal error handler is called outside the constructor of the session to
    // allow all signals be connectected and handled.
    const auto fatalErrorHandler = nx::utils::guarded(this,
        [this, errorCode]()
        {
            NX_DEBUG(this, "handleFatalErrorOccured(): delayed callback: called");
            m_connectionTimer.invalidate();
            m_reconnectHelper.reset();

            callInitialConnectionCallbackOnce(errorCode);
            emit q->finishedWithError(errorCode);
        });

    executeLater(fatalErrorHandler, this);
}

void Session::Private::gatherInitialConnectionInfo(int remainingTriesCount)
{
    if (remainingTriesCount <= 0)
    {
        handleFatalErrorOccured(RemoteConnectionErrorCode::genericNetworkError);
        return;
    }

    const auto logonData =
        [this]() -> std::optional<core::LogonData>
        {
            if (const auto localData = std::get_if<LocalConnectionData>(&m_connectionData))
                return core::localLogonData(localData->url, localData->credentials);

            const auto cloudData = std::get<CloudConnectionData>(m_connectionData);
            auto logonData = core::cloudLogonData(cloudData.cloudSystemId);
            if (!logonData)
                return {};

            const auto tokenInfo = details::PushIpcData::accessToken(
                cloudData.cloudSystemId.toStdString());
            logonData->accessToken = tokenInfo.accessToken;
            logonData->tokenExpiresAt = tokenInfo.expiresAt;

            const auto system = core::appContext()->systemFinder()->getSystem(
                cloudData.cloudSystemId);
            if (system->isOauthSupported())
                return logonData;

            if (cloudData.digestCredentials)
            {
                logonData->credentials.username = cloudData.digestCredentials->username;
                logonData->credentials.authToken = cloudData.digestCredentials->authToken;
                return logonData;
            }

            if (isDigestCloudAuthNeeded(system, logonData))
            {
                const auto lastPassword =
                    core::appContext()->coreSettings()->digestCloudPassword();
                if (!lastPassword.empty())
                {
                    logonData->credentials.username =
                        core::appContext()->cloudStatusWatcher()->cloudLogin().toStdString();
                    logonData->credentials.authToken =
                        {lastPassword, nx::network::http::AuthTokenType::password};
                }
            }

            return logonData;
        }();

    if (!logonData)
    {
        static constexpr auto kRequestPeriod = std::chrono::milliseconds(500);
        const auto getter = nx::utils::guarded(this,
            [this, remainingTriesCount]()
            {
                gatherInitialConnectionInfo(remainingTriesCount - 1);
            });
        executeDelayedParented(getter, kRequestPeriod, this);
        return;
    }

    const auto continueConnectionProcess = nx::utils::guarded(this,
        [this](const core::LogonData& logonData)
        {
            updateLogonData(logonData);
            if (!suspended())
                connectToServer();
        });

    if (const auto cloudData = std::get_if<CloudConnectionData>(&m_connectionData))
    {
        const auto system = core::appContext()->systemFinder()->getSystem(
            cloudData->cloudSystemId);
        if (isDigestCloudAuthNeeded(system, logonData))
        {
            const auto showPasswordRequest = nx::utils::guarded(this,
                [this, logonData, cloudData, system, continueConnectionProcess]()
                {
                    const auto login = core::appContext()->cloudStatusWatcher()->cloudLogin();
                    QVariantMap properties;
                    properties["login"] = login;
                    properties["systemName"] = system->name();
                    properties["cloudSystemId"] = cloudData->cloudSystemId;

                    const QPointer<Private> guard = this;
                    const auto password = nx::vms::client::mobile::QmlWrapperHelper::showScreen(
                        QUrl("qrc:qml/Nx/Screens/Cloud/DigestLogin.qml"), properties);

                    if (!guard)
                        return;

                    if (password.trimmed().isEmpty())
                    {
                        handleFatalErrorOccured(RemoteConnectionErrorCode::unauthorized);
                        return;
                    }

                    auto data = logonData.value();
                    data.credentials.username = login.toStdString();
                    data.credentials.authToken = nx::network::http::PasswordAuthToken(
                        password.toStdString());
                    continueConnectionProcess(data);
                });

            executeLaterInThread(showPasswordRequest, qApp->thread());
            return;
        }
    }

    continueConnectionProcess(logonData.value());
}

void Session::Private::handleConnectionClosed()
{
    if (NX_ASSERT(!connectionInProgress(m_state)))
        NX_DEBUG(this, "handleConnectionClosed(): handling, suspended is <%1>", suspended());
    else
        NX_DEBUG(this, "handleConnectionClosed(): connection can't be closed while connecting");

    if (!suspended())
        connectToServer();
}

void Session::Private::callInitialConnectionCallbackOnce(
    std::optional<RemoteConnectionErrorCode> errorCode)
{
    if (!m_initialConnectionCallback)
        return;

    NX_DEBUG(this, "callInitialConnectionCallbackOnce(): calling callback");

    m_initialConnectionCallback(errorCode);
    m_initialConnectionCallback = {};
}

void Session::Private::handleApplicationStateChanged(Qt::ApplicationState state)
{
    NX_DEBUG(this, "handleApplicationStateChanged(): state is <%1>", state);

    switch (state)
    {
        case Qt::ApplicationSuspended:
            if (!m_suspended)
                m_suspendedTimer.start();
            break;
        case Qt::ApplicationActive:
            m_suspendedTimer.stop();
            setSuspended(false);
            break;
        default:
            break;
    }
}

void Session::Private::handleInitialResourcesReceived()
{
    NX_DEBUG(this, "handleInitialResourcesReceived(): called");

    const bool oldWasConnected = m_wasConnected;
    setWasConnected();
    updateConnectionState();

    callInitialConnectionCallbackOnce();
    if (oldWasConnected)
        emit q->restored();
}

void Session::Private::updateSystemName(const QString& value)
{
    if (value == m_systemName || value.isEmpty())
        return;

    m_systemName = value;
    emit q->systemNameChanged();
}

void Session::Private::setConnectionVersion(const nx::utils::SoftwareVersion& value)
{
    if (value == m_connectionVersion)
        return;

    m_connectionVersion = value;
    emit q->connectedServerVersionChanged();
}

//-------------------------------------------------------------------------------------------------

Session::Holder Session::create(
    core::SystemContext* systemContext,
    const ConnectionData& connectionData,
    const QString& supposedSystemName,
    session::ConnectionCallback&& callback)
{
    return Session::Holder(
        new Session(systemContext, connectionData, supposedSystemName, std::move(callback)));
}

Session::~Session()
{
    NX_DEBUG(this, "~Session(): start: destroying session");

    // When called in ApplicationContext destructor the QnClientCoreModule is already destroyed.
    if (qnClientCoreModule)
    {
        if (const auto session = qnClientCoreModule->networkModule()->session())
        {
            const auto& localSystemId = d->localSystemId();
            const auto& userName = d->logonData().credentials.username;
            if (!hasBearerToken(localSystemId, userName))
            {
                // Absent credentials means that we don't want to preserve the remote session.
                session->setAutoTerminate(true);
            }

            if (!isCloudSession() && !hasStoredToken(localSystemId, userName))
                core::appContext()->coreSettings()->lastConnection = {};
        }
    }
    d->resetAddress();
    d->setSuspended(true); //< Disconnects from current server and prevents reconnecting.

    const auto pool = resourcePool();
    const auto remoteResources = pool->getResourcesWithFlag(Qn::remote);
    pool->removeResources(remoteResources);

    NX_DEBUG(this, "~Session(): end");
}

nx::network::SocketAddress Session::address() const
{
    return d->logonData().address;
}

Session::ConnectionState Session::connectionState() const
{
    return d->connectionState();
}

bool Session::restoringConnection() const
{
    return d->wasConnected() && d->connectionState() < Session::ConnectionState::ready;
}

bool Session::wasConnected() const
{
    return d->wasConnected();
}

bool Session::isCloudSession() const
{
    return d->logonData().userType == nx::vms::api::UserType::cloud;
}

QString Session::systemName() const
{
    return d->systemName();
}

Session::ConnectionData Session::connectionData() const
{
    return d->connectionData();
}

nx::utils::SoftwareVersion Session::connectedServerVersion() const
{
    return d->connectionVersion();
}

nx::Uuid Session::localSystemId() const
{
    return d->localSystemId();
}

QString Session::currentUser() const
{
    return d->currentUser();
}

void Session::setSuspended(bool suspended)
{
    d->setSuspended(suspended);
}

Session::Session(
    core::SystemContext* systemContext,
    const ConnectionData& connectionData,
    const QString& supposedSystemName,
    session::ConnectionCallback&& callback,
    QObject* parent)
    :
    base_type(parent),
    SystemContextAware(systemContext),
    d(new Private(connectionData, supposedSystemName, std::move(callback), this))
{
    NX_DEBUG(this, "Session(): Creating new session");

    connect(this, &Session::connectionStateChanged, this, &Session::restoringConnectionChanged);
    connect(this, &Session::wasConnectedChanged, this, &Session::restoringConnectionChanged);

    d->gatherInitialConnectionInfo();
}

QString toString(Session::ConnectionState state)
{
    switch (state)
    {
        case Session::ConnectionState::disconnected:
            return "disconnected";
        case Session::ConnectionState::connecting:
            return "connecting";
        case Session::ConnectionState::loading:
            return "loading";
        case Session::ConnectionState::reconnecting:
            return "reconnecting";
        case Session::ConnectionState::ready:
            return "ready";
        case Session::ConnectionState::suspended:
            return "suspended";
        default:
            return "unexpected";
    }
}

} // namespace nx::vms:client::mobile
