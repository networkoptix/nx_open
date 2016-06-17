#include "connection_manager.h"

#include <QtCore/QUrl>
#include <QtGui/QGuiApplication>
#include <QtCore/QMetaEnum>

#include <core/resource_management/resource_pool.h>
#include <core/core_settings.h>
#include <api/abstract_connection.h>
#include <api/app_server_connection.h>
#include <api/session_manager.h>
#include <api/global_settings.h>
#include <api/runtime_info_manager.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <common/common_module.h>
#include <mobile_client/mobile_client_message_processor.h>
#include <mobile_client/mobile_client_settings.h>
#include <watchers/user_watcher.h>

namespace {

    const QnSoftwareVersion minimalSupportedVersion(2, 5, 0, 0);

    enum { kInvalidHandle = -1 };

    bool isCompatibleProducts(const QString &product1, const QString &product2) {
        return  product1.isEmpty() ||
                product2.isEmpty() ||
                product1.left(product1.indexOf(lit("_"))) == product2.left(product2.indexOf(lit("_")));
    }

}

class QnConnectionManagerPrivate : public Connective<QObject> {
    QnConnectionManager *q_ptr;
    Q_DECLARE_PUBLIC(QnConnectionManager)

    typedef Connective<QObject> base_type;

public:
    QnConnectionManagerPrivate(QnConnectionManager *parent);

    void at_applicationStateChanged(Qt::ApplicationState state);

    void suspend();
    void resume();

    void doConnect();
    void doDisconnect(bool force = false);

    void updateConnectionState();

    void setUrl(const QUrl& url);

    void storeConnection(
            const QString& systemName,
            const QUrl& url,
            bool storePassword);

public:
    QUrl url;
    bool suspended;
    QTimer *suspendTimer;
    int connectionHandle;
    QnConnectionManager::State connectionState;
};

QnConnectionManager::QnConnectionManager(QObject *parent) :
    QObject(parent),
    d_ptr(new QnConnectionManagerPrivate(this))
{
    Q_D(QnConnectionManager);

    connect(qnCommon, &QnCommonModule::systemNameChanged, this, &QnConnectionManager::systemNameChanged);

    connect(qnClientMessageProcessor, &QnMobileClientMessageProcessor::initialResourcesReceived,
            this, &QnConnectionManager::initialResourcesReceived);
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionOpened,
            d, &QnConnectionManagerPrivate::updateConnectionState);
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionClosed,
            d, &QnConnectionManagerPrivate::updateConnectionState);

    connect(this, &QnConnectionManager::currentUrlChanged, this, &QnConnectionManager::currentHostChanged);
    connect(this, &QnConnectionManager::currentUrlChanged, this, &QnConnectionManager::currentLoginChanged);
    connect(this, &QnConnectionManager::currentUrlChanged, this, &QnConnectionManager::currentPasswordChanged);
    connect(this, &QnConnectionManager::connectionStateChanged, this, &QnConnectionManager::isOnlineChanged);
}

QnConnectionManager::~QnConnectionManager() {
}

QString QnConnectionManager::systemName() const {
    return qnCommon->localSystemName();
}

QnConnectionManager::State QnConnectionManager::connectionState() const
{
    Q_D(const QnConnectionManager);
    return d->connectionState;
}

bool QnConnectionManager::isOnline() const
{
    Q_D(const QnConnectionManager);
    return d->connectionState == Connected || d->connectionState == Suspended;
}

int QnConnectionManager::defaultServerPort() const {
    return DEFAULT_APPSERVER_PORT;
}

QUrl QnConnectionManager::currentUrl() const
{
    Q_D(const QnConnectionManager);
    return d->url.isValid() ? d->url : QUrl();
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

void QnConnectionManager::connectToServer(const QUrl &url)
{
    Q_D(QnConnectionManager);

    if (connectionState() != QnConnectionManager::Disconnected)
        disconnectFromServer(false);

    QUrl fixedUrl(url);
    if (fixedUrl.port() == -1)
        fixedUrl.setPort(defaultServerPort());

    d->setUrl(fixedUrl);
    d->doConnect();
}

void QnConnectionManager::disconnectFromServer(bool force) {
    Q_D(QnConnectionManager);

    if (d->connectionHandle != kInvalidHandle)
    {
        d->connectionHandle = kInvalidHandle;
        d->setUrl(QUrl());
        d->updateConnectionState();
        return;
    }

    if (d->connectionState == Disconnected || d->connectionState == Connecting)
        return;

    d->doDisconnect(force);

    d->setUrl(QUrl());
    QnAppServerConnectionFactory::setCurrentVersion(QnSoftwareVersion());
    qnCommon->instance<QnUserWatcher>()->setUserName(QString());

    // TODO: #dklychkov Move it to a better place
    QnResourceList remoteResources = qnResPool->getResourcesWithFlag(Qn::remote);
    qnResPool->removeResources(remoteResources);
}

QnConnectionManagerPrivate::QnConnectionManagerPrivate(QnConnectionManager *parent)
    : base_type(parent)
    , q_ptr(parent)
    , suspended(false)
    , suspendTimer(new QTimer(this))
    , connectionHandle(kInvalidHandle)
    , connectionState(QnConnectionManager::Disconnected)
{
    enum { suspendInterval = 2 * 60 * 1000 };
    suspendTimer->setInterval(suspendInterval);
    suspendTimer->setSingleShot(true);

    connect(suspendTimer, &QTimer::timeout, this, &QnConnectionManagerPrivate::suspend);
    connect(qApp, &QGuiApplication::applicationStateChanged, this, &QnConnectionManagerPrivate::at_applicationStateChanged);
}

void QnConnectionManagerPrivate::at_applicationStateChanged(Qt::ApplicationState state) {
    switch (state) {
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

void QnConnectionManagerPrivate::suspend() {
    if (suspended)
        return;

    suspended = true;

    doDisconnect();
}

void QnConnectionManagerPrivate::resume() {
    suspendTimer->stop();

    if (!suspended)
        return;

    suspended = false;

    doConnect();
}

void QnConnectionManagerPrivate::doConnect() {
    if (!url.isValid())
    {
        Q_Q(QnConnectionManager);
        updateConnectionState();
        emit q->connectionFailed(QnConnectionManager::NetworkError, QVariant());
        return;
    }

    qnCommon->updateRunningInstanceGuid();

    QnEc2ConnectionRequestResult *result = new QnEc2ConnectionRequestResult(this);
    connectionHandle = QnAppServerConnectionFactory::ec2ConnectionFactory()->connect(
        url, ec2::ApiClientInfoData(), result, &QnEc2ConnectionRequestResult::processEc2Reply);

    updateConnectionState();

    connect(result, &QnEc2ConnectionRequestResult::replyProcessed, this, [this, result]() {
        result->deleteLater();

        if (connectionHandle != result->handle())
            return;

        connectionHandle = kInvalidHandle;

        ec2::ErrorCode errorCode = ec2::ErrorCode(result->status());

        QnConnectionInfo connectionInfo = result->reply<QnConnectionInfo>();
        auto status = QnConnectionManager::Success;
        QVariant infoParameter;

        if (errorCode == ec2::ErrorCode::unauthorized) {
            status = QnConnectionManager::Unauthorized;
        } else if (errorCode != ec2::ErrorCode::ok) {
            status = QnConnectionManager::NetworkError;
        } else if (!isCompatibleProducts(connectionInfo.brand, QnAppInfo::productNameShort())) {
            status = QnConnectionManager::InvalidServer;
        } else if (connectionInfo.version < minimalSupportedVersion) {
            status = QnConnectionManager::InvalidVersion;
            infoParameter = connectionInfo.version.toString(QnSoftwareVersion::BugfixFormat);
        }

        Q_Q(QnConnectionManager);

        if (status != QnConnectionManager::Success) {
            updateConnectionState();
            emit q->connectionFailed(status, infoParameter);
            return;
        }

        ec2::AbstractECConnectionPtr ec2Connection = result->connection();

        QnAppServerConnectionFactory::setUrl(url);
        QnAppServerConnectionFactory::setEc2Connection(ec2Connection);
        QnAppServerConnectionFactory::setCurrentVersion(connectionInfo.version);

        QnMobileClientMessageProcessor::instance()->init(ec2Connection);

        QnSessionManager::instance()->start();

        connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoChanged, this, [this, ec2Connection](const QnPeerRuntimeInfo &info) {
            if (info.uuid == qnCommon->moduleGUID())
                ec2Connection->sendRuntimeData(info.data);
        });

        connect(
            ec2Connection->getTimeNotificationManager().get(),
            &ec2::AbstractTimeNotificationManager::timeChanged,
            QnSyncTime::instance(),
            static_cast<void(QnSyncTime::*)(qint64)>(&QnSyncTime::updateTime));

        qnCommon->setLocalSystemName(connectionInfo.systemName);
        qnCommon->instance<QnUserWatcher>()->setUserName(url.userName());

        updateConnectionState();

        storeConnection(connectionInfo.systemName, url, true);
        qnSettings->setLastUsedSystemId(connectionInfo.systemName);
        url.setPassword(QString());
        qnSettings->setLastUsedUrl(url);
    });
}

void QnConnectionManagerPrivate::doDisconnect(bool force) {
    if (!force)
        qnGlobalSettings->synchronizeNow();

    disconnect(QnRuntimeInfoManager::instance(), nullptr, this, nullptr);

    QnMobileClientMessageProcessor::instance()->init(NULL);
    QnAppServerConnectionFactory::setUrl(QUrl());
    QnAppServerConnectionFactory::setEc2Connection(NULL);
    QnSessionManager::instance()->stop();

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
        newState = QnConnectionManager::Connecting;
    }
    else
    {
        switch (qnClientMessageProcessor->connectionState())
        {
        case QnConnectionState::Connecting:
        case QnConnectionState::Reconnecting:
            newState = QnConnectionManager::Connecting;
            break;
        case QnConnectionState::Connected:
        case QnConnectionState::Ready:
            newState = QnConnectionManager::Connected;
            break;
        default:
            newState = QnConnectionManager::Disconnected;
            break;
        }
    }

    if (newState == connectionState)
        return;

    connectionState = newState;

    Q_Q(QnConnectionManager);
    emit q->connectionStateChanged();
}

void QnConnectionManagerPrivate::setUrl(const QUrl& url)
{
    if (this->url == url)
        return;

    Q_Q(QnConnectionManager);
    this->url = url;
    emit q->currentUrlChanged();
}

void QnConnectionManagerPrivate::storeConnection(
        const QString& systemName,
        const QUrl& url,
        bool storePassword)
{
    auto lastConnections = qnCoreSettings->recentUserConnections();

    const auto password = storePassword ? url.password()
                                        : QString();
    const QnUserRecentConnectionData connectionInfo =
        { systemName, url.userName(), password, storePassword };

    auto connectionEqual = [connectionInfo](const QnUserRecentConnectionData& connection)
    {
        return connection.systemName == connectionInfo.systemName;
    };
    lastConnections.erase(std::remove_if(lastConnections.begin(), lastConnections.end(), connectionEqual),
                          lastConnections.end());
    lastConnections.prepend(connectionInfo);

    qnCoreSettings->setRecentUserConnections(lastConnections);
}
