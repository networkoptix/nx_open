#include "connection_manager.h"

#include <QtCore/QUrl>
#include <QtGui/QGuiApplication>

#include <core/resource_management/resource_pool.h>
#include "api/abstract_connection.h"
#include "api/app_server_connection.h"
#include "api/session_manager.h"
#include "api/model/connection_info.h"
#include "api/global_settings.h"
#include "api/runtime_info_manager.h"
#include "mobile_client/mobile_client_message_processor.h"
#include "utils/common/app_info.h"
#include "utils/common/synctime.h"
#include "mobile_client/mobile_client_settings.h"
#include "common/common_module.h"
#include "watchers/user_watcher.h"

namespace {

    const QnSoftwareVersion minimalSupportedVersion(2, 5, 0, 0);

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

public:
    QUrl url;
    bool suspended;
    QTimer *suspendTimer;
};

QnConnectionManager::QnConnectionManager(QObject *parent) :
    QObject(parent),
    d_ptr(new QnConnectionManagerPrivate(this))
{
    connect(qnCommon, &QnCommonModule::systemNameChanged, this, &QnConnectionManager::systemNameChanged);
    connect(qnClientMessageProcessor, &QnMobileClientMessageProcessor::initialResourcesReceived, this, &QnConnectionManager::initialResourcesReceived);
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionOpened, this, &QnConnectionManager::connectionStateChanged);
}

QnConnectionManager::~QnConnectionManager() {
}

QString QnConnectionManager::systemName() const {
    return qnCommon->localSystemName();
}

QnConnectionManager::State QnConnectionManager::connectionState() const {
    Q_D(const QnConnectionManager);
    if (d->suspended)
        return Suspended;

    switch (qnClientMessageProcessor->connectionState()) {
    case QnConnectionState::Connecting:
    case QnConnectionState::Reconnecting:
        return Connecting;
    case QnConnectionState::Connected:
    case QnConnectionState::Ready:
        return Connected;
    default:
        return Disconnected;
    }
}

void QnConnectionManager::connectToServer(const QUrl &url) {
    Q_D(QnConnectionManager);

    if (!url.isValid())
        return;

    if (connectionState() != QnConnectionManager::Disconnected)
        disconnectFromServer(false);

    d->url = url;

    d->doConnect();
}

void QnConnectionManager::disconnectFromServer(bool force) {
    Q_D(QnConnectionManager);

    d->doDisconnect(force);

    d->url = QUrl();
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
        return;

    qnCommon->updateRunningInstanceGuid();

    QnEc2ConnectionRequestResult *result = new QnEc2ConnectionRequestResult(this);
    QnAppServerConnectionFactory::ec2ConnectionFactory()->connect(
        url, ec2::ApiClientInfoData(), result, &QnEc2ConnectionRequestResult::processEc2Reply);

    connect(result, &QnEc2ConnectionRequestResult::replyProcessed, this, [this, result]() {
        result->deleteLater();

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

        connect(ec2Connection->getTimeManager().get(), &ec2::AbstractTimeManager::timeChanged, QnSyncTime::instance(), static_cast<void(QnSyncTime::*)(qint64)>(&QnSyncTime::updateTime));

        qnCommon->setLocalSystemName(connectionInfo.systemName);
        qnCommon->instance<QnUserWatcher>()->setUserName(url.userName());

        emit q->connectionStateChanged();
    });
}

void QnConnectionManagerPrivate::doDisconnect(bool force) {
    if (!force)
        QnGlobalSettings::instance()->synchronizeNow();

    disconnect(QnRuntimeInfoManager::instance(), nullptr, this, nullptr);

    QnMobileClientMessageProcessor::instance()->init(NULL);
    QnAppServerConnectionFactory::setUrl(QUrl());
    QnAppServerConnectionFactory::setEc2Connection(NULL);
    QnSessionManager::instance()->stop();

    Q_Q(QnConnectionManager);
    emit q->connectionStateChanged();
}
