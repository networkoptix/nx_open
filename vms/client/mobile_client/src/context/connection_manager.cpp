#include "connection_manager.h"

#include <QtGui/QGuiApplication>
#include <QtCore/QMetaEnum>

#include <client_core/connection_context_aware.h>
#include <client_core/client_core_module.h>
#include <client_core/client_core_settings.h>

#include <mobile_client/mobile_client_message_processor.h>
#include <mobile_client/mobile_client_settings.h>
#include <mobile_client/mobile_client_module.h>

#include <core/resource_management/resource_pool.h>
#include <api/abstract_connection.h>
#include <api/app_server_connection.h>
#include <api/session_manager.h>
#include <api/global_settings.h>
#include <api/runtime_info_manager.h>
#include <utils/common/synctime.h>
#include <utils/common/app_info.h>
#include <utils/common/delayed.h>
#include <common/common_module.h>
#include <network/connection_validator.h>

#include <nx_ec/dummy_handler.h>

#include <nx/client/core/utils/reconnect_helper.h>
#include <nx/client/core/watchers/user_watcher.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <network/system_helpers.h>
#include <helpers/system_weight_helper.h>
#include <helpers/system_helpers.h>
#include <settings/last_connection.h>
#include <nx/utils/log/log.h>

namespace {

    const QString kLiteClientConnectionScheme = lit("liteclient");
    enum { kInvalidHandle = -1 };

    QnConnectionManager::ConnectionType connectionTypeForUrl(const nx::utils::Url& url)
    {
        if (nx::network::SocketGlobals::addressResolver().isCloudHostName(url.host())
            || url.scheme() == QnConnectionManager::kCloudConnectionScheme)
        {
            return QnConnectionManager::CloudConnection;
        }

        if (url.scheme() == kLiteClientConnectionScheme)
            return QnConnectionManager::LiteClientConnection;

        return QnConnectionManager::NormalConnection;
    }

} // namespace

const QString QnConnectionManager::kCloudConnectionScheme = lit("cloud");

class QnConnectionManagerPrivate : public Connective<QObject>, public QnConnectionContextAware
{
    QnConnectionManager* q_ptr;
    Q_DECLARE_PUBLIC(QnConnectionManager)

    typedef Connective<QObject> base_type;

public:
    QnConnectionManagerPrivate(QnConnectionManager* parent);

    void at_applicationStateChanged(Qt::ApplicationState state);

    void suspend();
    void resume();

    bool doConnect(bool restoringConnection);
    void doDisconnect();
    void tryResotreConnection();

    void updateConnectionState();

    void setUrl(const nx::utils::Url &url);

    void setSystemName(const QString& systemName);

    void updateRestoringConnectionValue();
    void setRestoringConnectionFlag(bool value);
    bool connectToServerInternal(const nx::utils::Url& url, bool restoringConnection);

public:
    QTimer setRestoringConnectionTimer;
    bool restoringConnectionFlag = false;

    nx::utils::Url url;
    QScopedPointer<nx::vms::client::core::ReconnectHelper> reconnectHelper;
    QString systemName;
    bool suspended = false;
    bool wasConnected = false;
    QTimer* suspendTimer = nullptr;
    int connectionHandle = kInvalidHandle;
    QnConnectionManager::State connectionState = QnConnectionManager::Disconnected;
    nx::utils::SoftwareVersion connectionVersion;
    QnConnectionManager::ConnectionType connectionType = QnConnectionManager::NormalConnection;
};

QnConnectionManager::QnConnectionManager(QObject* parent):
    QObject(parent),
    d_ptr(new QnConnectionManagerPrivate(this))
{
    Q_D(QnConnectionManager);

    QPointer<QnGlobalSettings> settings(qnGlobalSettings);
    connect(settings.data(), &QnGlobalSettings::systemNameChanged, this,
        [d, settings]()
        {
            /* 2.6 servers use another property name for systemName, so in 3.0 it's always empty.
               However we fill system name in doConnect().
               This value won't change, but it's not so critical. */
            const auto systemName = settings->systemName();
            if (!systemName.isEmpty())
                d->setSystemName(systemName);
        });

    connect(qnClientMessageProcessor, &QnMobileClientMessageProcessor::initialResourcesReceived, d,
        [this, d]()
        {
            const bool initialConnection = !d->wasConnected;
            emit connected(initialConnection);
            d->wasConnected = true;
            d->updateConnectionState();
            d->updateRestoringConnectionValue();
        });
    connect(qnClientMessageProcessor->connectionStatus(), &QnClientConnectionStatus::stateChanged,
        d, &QnConnectionManagerPrivate::updateConnectionState);
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionClosed,
        d, &QnConnectionManagerPrivate::tryResotreConnection);
    connect(this, &QnConnectionManager::connectionStateChanged,
        d, &QnConnectionManagerPrivate::updateRestoringConnectionValue);

    connect(this, &QnConnectionManager::currentUrlChanged,
        this, &QnConnectionManager::currentHostChanged);
    connect(this, &QnConnectionManager::currentUrlChanged,
        this, &QnConnectionManager::currentLoginChanged);
    connect(this, &QnConnectionManager::currentUrlChanged,
        this, &QnConnectionManager::currentPasswordChanged);
    connect(this, &QnConnectionManager::connectionStateChanged,
        this, &QnConnectionManager::isOnlineChanged);
}

QnConnectionManager::~QnConnectionManager()
{
    disconnectFromServer();
}

QString QnConnectionManager::systemName() const
{
    Q_D(const QnConnectionManager);
    return d->systemName;
}

QnConnectionManager::State QnConnectionManager::connectionState() const
{
    Q_D(const QnConnectionManager);
    return d->connectionState;
}

bool QnConnectionManager::isOnline() const
{
    Q_D(const QnConnectionManager);
    return d->connectionState >= Connected && d->wasConnected;
}

QnConnectionManager::ConnectionType QnConnectionManager::connectionType() const
{
    Q_D(const QnConnectionManager);
    return d->connectionType;
}

int QnConnectionManager::defaultServerPort() const
{
    return helpers::kDefaultConnectionPort;
}

nx::utils::Url QnConnectionManager::currentUrl() const
{
    Q_D(const QnConnectionManager);
    return d->url.isValid() ? d->url : nx::utils::Url();
}

QString QnConnectionManager::currentHost() const
{
    Q_D(const QnConnectionManager);
    return d->url.isValid() ? lit("%1:%2").arg(d->url.host()).arg(d->url.port(defaultServerPort())) : QString();
}

QString QnConnectionManager::currentLogin() const
{
    Q_D(const QnConnectionManager);
    return d->url.isValid() ? d->url.userName() : QString();
}

QString QnConnectionManager::currentPassword() const
{
    Q_D(const QnConnectionManager);
    return d->url.isValid() ? d->url.password() : QString();
}

nx::utils::SoftwareVersion QnConnectionManager::connectionVersion() const
{
    Q_D(const QnConnectionManager);
    return d->connectionVersion;
}

bool QnConnectionManager::restoringConnection() const
{
    Q_D(const QnConnectionManager);
    return d->restoringConnectionFlag;
}

bool QnConnectionManager::connectToServer(const nx::utils::Url& url)
{
    Q_D(QnConnectionManager);
    return d->connectToServerInternal(url, false);
}

bool QnConnectionManager::connectToServer(
    const nx::utils::Url &url,
    const QString& userName,
    const QString& password,
    bool cloudConnection)
{
    auto urlWithAuth = url;
    urlWithAuth.setUserName(userName);
    urlWithAuth.setPassword(password);
    if (cloudConnection)
        urlWithAuth.setScheme(kCloudConnectionScheme);
    return connectToServer(urlWithAuth);
}

bool QnConnectionManager::connectByUserInput(
    const QString& address,
    const QString& userName,
    const QString& password)
{
    nx::utils::Url url = nx::utils::Url::fromUserInput(address);
    if (url.port() <= 0)
        url.setPort(helpers::kDefaultConnectionPort);

    url.setUserName(userName);
    url.setPassword(password);
    return connectToServer(url);
}

void QnConnectionManager::disconnectFromServer()
{
    Q_D(QnConnectionManager);

    if (d->connectionHandle != kInvalidHandle)
    {
        d->connectionHandle = kInvalidHandle;
        d->setUrl(nx::utils::Url());
        d->updateConnectionState();
        return;
    }

    if (d->connectionState == Disconnected)
        return;

    d->wasConnected = false;
    d->doDisconnect();

    d->setUrl(nx::utils::Url());
    commonModule()->instance<nx::vms::client::core::UserWatcher>()->setUserName(QString());

    // TODO: #dklychkov Move it to a better place
    QnResourceList remoteResources = resourcePool()->getResourcesWithFlag(Qn::remote);
    resourcePool()->removeResources(remoteResources);
}

QnConnectionManagerPrivate::QnConnectionManagerPrivate(QnConnectionManager* parent):
    base_type(parent),
    q_ptr(parent),
    suspendTimer(new QTimer(this))
{
    enum { suspendInterval = 2 * 60 * 1000 };
    suspendTimer->setInterval(suspendInterval);
    suspendTimer->setSingleShot(true);

    connect(suspendTimer, &QTimer::timeout, this, &QnConnectionManagerPrivate::suspend);
    connect(qApp, &QGuiApplication::applicationStateChanged, this, &QnConnectionManagerPrivate::at_applicationStateChanged);

    static constexpr int kDelayMs = 20 * 1000;
    setRestoringConnectionTimer.setSingleShot(true);
    setRestoringConnectionTimer.setInterval(kDelayMs);
    connect(&setRestoringConnectionTimer, &QTimer::timeout,
        this, [this](){ setRestoringConnectionFlag(true); });
}

void QnConnectionManagerPrivate::at_applicationStateChanged(Qt::ApplicationState state)
{
    switch (state)
    {
        case Qt::ApplicationActive:
            resume();
            break;
        case Qt::ApplicationSuspended:
            if (!suspended)
                suspendTimer->start();
            break;
        default:
            break;
    }
}

void QnConnectionManagerPrivate::suspend()
{
    if (suspended)
        return;

    suspended = true;

    doDisconnect();
}

void QnConnectionManagerPrivate::resume()
{
    suspendTimer->stop();

    if (!suspended)
        return;

    suspended = false;

    doConnect(false);
}

bool QnConnectionManagerPrivate::doConnect(bool restoringConnection)
{
    NX_DEBUG(this, lm("doConnect() BEGIN: url: %1").arg(url.toString()));
    if (!url.isValid() || url.host().isEmpty())
    {
        Q_Q(QnConnectionManager);
        updateConnectionState();
        emit q->connectionFailed(Qn::NetworkErrorConnectionResult, QVariant());
        NX_DEBUG(this, lm("doConnect() END: Invalid URL"));
        return false;
    }

    commonModule()->updateRunningInstanceGuid();

    auto connectUrl = url;
    connectUrl.setScheme(nx::network::http::kSecureUrlSchemeName);

    auto result = new QnEc2ConnectionRequestResult(this);
    connectionHandle = qnClientCoreModule->connectionFactory()->connect(
        connectUrl,
        nx::vms::api::ClientInfoData(),
        result,
        &QnEc2ConnectionRequestResult::processEc2Reply);

    updateConnectionState();

    if (connectionHandle == kInvalidHandle)
    {
        delete result;
        return false;
    }

    connect(result, &QnEc2ConnectionRequestResult::replyProcessed, this,
        [this, result, connectUrl, restoringConnection]()
        {
            result->deleteLater();

            if (connectionHandle != result->handle())
            {
                NX_DEBUG(this, lm("doConnect() Invalid handle"));
                return;
            }

            connectionHandle = kInvalidHandle;

            const auto errorCode = ec2::ErrorCode(result->status());
            const auto connectionInfo = result->reply<QnConnectionInfo>();

            auto status = QnConnectionValidator::validateConnection(connectionInfo, errorCode);
            QVariant infoParameter;

            if (status == Qn::IncompatibleVersionConnectionResult)
            {
                infoParameter = connectionInfo.version.toString(
                    nx::utils::SoftwareVersion::BugfixFormat);
            }
            else if(status == Qn::IncompatibleCloudHostConnectionResult)
            {
                /* Ignore this kind of error because Mobile Client does not have
                   compatibility versions mechanism. Thus without it beta testers will have to
                   manually downgrade the app to connect to ther release systems. */
                status = Qn::SuccessConnectionResult;
            }

            Q_Q(QnConnectionManager);

            if (status != Qn::SuccessConnectionResult)
            {
                updateConnectionState();
                if (restoringConnection)
                    tryResotreConnection();
                else
                    emit q->connectionFailed(status, infoParameter);

                NX_DEBUG(this, lm("doConnect() END: Bad status"));
                return;
            }

            reconnectHelper.reset();

            const auto ec2Connection = result->connection();
            QnAppServerConnectionFactory::setEc2Connection(ec2Connection);

            qnMobileClientMessageProcessor->init(ec2Connection);

            commonModule()->sessionManager()->start();

            connect(runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoChanged,
                this, [this, ec2Connection](const QnPeerRuntimeInfo& info)
                {
                    if (info.uuid == commonModule()->moduleGUID())
                    {
                        ec2Connection->getMiscManager(Qn::kSystemAccess)->saveRuntimeInfo(
                            info.data,
                            ec2::DummyHandler::instance(),
                            &ec2::DummyHandler::onRequestDone);
                    }
                });

            commonModule()->instance<nx::vms::client::core::UserWatcher>()->setUserName(
                connectionInfo.effectiveUserName.isEmpty()
                    ? url.userName()
                    : connectionInfo.effectiveUserName);

            updateConnectionState();
            setSystemName(connectionInfo.systemName);

            if (!restoringConnection)
            {
                const auto localId = helpers::getLocalSystemId(connectionInfo);
                using namespace nx::vms::client::core::helpers;
                if (connectionTypeForUrl(url) != QnConnectionManager::CloudConnection)
                {
                    nx::vms::common::Credentials credentials(url.userName(), {});
                    if (qnSettings->savePasswords())
                        credentials.password = url.password();
                    storeCredentials(localId, credentials);
                    storeConnection(localId, connectionInfo.systemName, url);
                    updateWeightData(localId);
                    qnClientCoreSettings->save();
                }

                LastConnectionData connectionData{
                    connectionInfo.systemName,
                    url.cleanUrl(),
                    nx::vms::client::core::EncodedCredentials(url)};
                qnSettings->setLastUsedConnection(connectionData);
                qnSettings->save();
            }

            connectionVersion = connectionInfo.version;
            emit q->connectionVersionChanged();
        });

    NX_DEBUG(this, lm("doConnect() END"));
    return true;
}

void QnConnectionManagerPrivate::tryResotreConnection()
{
    if (!wasConnected)
        return;

    if (connectionState == QnConnectionManager::Connecting
        || connectionState == QnConnectionManager::Ready)
    {
        return;
    }

    if (!reconnectHelper)
        reconnectHelper.reset(new nx::vms::client::core::ReconnectHelper());

    if (reconnectHelper->servers().size() <= 1)
        return; // We are already reconnecting.

    reconnectHelper->next();
    const auto currentUrl = reconnectHelper->currentUrl();
    if (currentUrl.isValid())
    {
        connectToServerInternal(currentUrl, true);
        return;
    }

    static constexpr auto kReconnectDelayMs = 3000;
    const auto callReconnect = [this] { tryResotreConnection(); };
    executeDelayedParented(callReconnect, kReconnectDelayMs, reconnectHelper.data());
}

void QnConnectionManagerPrivate::doDisconnect()
{
    Q_Q(QnConnectionManager);

    reconnectHelper.reset();
    qnGlobalSettings->synchronizeNow();

    disconnect(runtimeInfoManager(), nullptr, this, nullptr);

    qnMobileClientMessageProcessor->init(nullptr);
    QnAppServerConnectionFactory::setEc2Connection(nullptr);
    commonModule()->sessionManager()->stop();

    setSystemName(QString());
    connectionVersion = {};
    emit q->connectionVersionChanged();

    updateConnectionState();
}

void QnConnectionManagerPrivate::updateConnectionState()
{
    QnConnectionManager::State newState = connectionState;

    if (suspended)
    {
        newState = QnConnectionManager::Suspended;
    }
    else if (connectionHandle != kInvalidHandle)
    {
        newState = wasConnected
            ? QnConnectionManager::Reconnecting
            : QnConnectionManager::Connecting;
    }
    else
    {
        newState = static_cast<QnConnectionManager::State>(
            qnClientMessageProcessor->connectionStatus()->state());
    }

    if (newState == connectionState)
        return;

    connectionState = newState;

    Q_Q(QnConnectionManager);
    emit q->connectionStateChanged();
}

void QnConnectionManagerPrivate::setUrl(const nx::utils::Url& url)
{
    if (this->url == url)
        return;

    Q_Q(QnConnectionManager);

    this->url = url;
    emit q->currentUrlChanged();

    const auto newConnectionType = connectionTypeForUrl(url);
    if (connectionType != newConnectionType)
    {
        connectionType = newConnectionType;
        emit q->connectionTypeChanged();
    }
}

void QnConnectionManagerPrivate::setSystemName(const QString& systemName)
{
    if (this->systemName == systemName)
        return;

    Q_Q(QnConnectionManager);

    this->systemName = systemName;
    emit q->systemNameChanged(systemName);
}

void QnConnectionManagerPrivate::setRestoringConnectionFlag(bool value)
{
    if (restoringConnectionFlag == value)
        return;

    restoringConnectionFlag = value;

    Q_Q(QnConnectionManager);
    emit q->restoringConnectionChanged();
}

void QnConnectionManagerPrivate::updateRestoringConnectionValue()
{
    const bool restoring = wasConnected
        && (connectionState == QnConnectionManager::Connecting
            || connectionState == QnConnectionManager::Disconnected
            || connectionState == QnConnectionManager::Reconnecting);

    if (restoring)
    {
        setRestoringConnectionTimer.start();
        return;
    }

    setRestoringConnectionFlag(false);
    setRestoringConnectionTimer.stop();
}

bool QnConnectionManagerPrivate::connectToServerInternal(
    const nx::utils::Url& url,
    bool restoringConnection)
{
    Q_Q(QnConnectionManager);
    if (!restoringConnection)
    {
        setRestoringConnectionTimer.stop();
        if (connectionState != QnConnectionManager::Disconnected)
            q->disconnectFromServer();
    }

    nx::utils::Url actualUrl = url;
    if (actualUrl.port() == -1)
        actualUrl.setPort(q->defaultServerPort());
    actualUrl.setUserName(actualUrl.userName().toLower());
    setUrl(actualUrl);
    return doConnect(restoringConnection);
}
