#include "connection_manager.h"

#include <QtCore/QUrl>
#include <QtGui/QGuiApplication>
#include <QtCore/QMetaEnum>

#include <core/resource_management/resource_pool.h>
#include "api/abstract_connection.h"
#include "api/app_server_connection.h"
#include "api/session_manager.h"
#include "api/global_settings.h"
#include "api/runtime_info_manager.h"
#include "../mobile_client/mobile_client_message_processor.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include <common/common_module.h>
#include "../watchers/user_watcher.h"
#include <nx_ec/data/api_videowall_data.h>
#include <nx_ec/data/api_conversion_functions.h>

#define OUTPUT_PREFIX "mobile_client[connection_manager]: "
#include <nx/utils/debug_utils.h>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtGui/QKeyEvent>

namespace {

    const QnSoftwareVersion minimalSupportedVersion(2, 5, 0, 0);

    enum { kInvalidHandle = -1 };

    bool isCompatibleProducts(const QString &product1, const QString &product2) {
        return  product1.isEmpty() ||
                product2.isEmpty() ||
                product1.left(product1.indexOf(lit("_"))) == product2.left(product2.indexOf(lit("_")));
    }

    #define PARAM_KEY(KEY) const QLatin1String KEY##Key(BOOST_PP_STRINGIZE(KEY));
    PARAM_KEY(sequence)
    PARAM_KEY(pcUuid)
    PARAM_KEY(uuid)
    PARAM_KEY(zoomUuid)
    PARAM_KEY(resource)
    PARAM_KEY(value)
    PARAM_KEY(role)
    PARAM_KEY(position)
    PARAM_KEY(rotation)
    PARAM_KEY(speed)
    PARAM_KEY(geometry)
    PARAM_KEY(zoomRect)
    PARAM_KEY(checkedButtons)
}

class QnConnectionManagerPrivate : public Connective<QObject> {
    QnConnectionManager *q_ptr;
    Q_DECLARE_PUBLIC(QnConnectionManager)

    typedef Connective<QObject> base_type;

private:
    void handleVideowallMessage(
        const QnVideoWallControlMessage& message,
        const QnUuid& controllerUuid = QnUuid(),
        qint64 sequence = -1);

public:
    QnConnectionManagerPrivate(QnConnectionManager *parent);

    void at_applicationStateChanged(Qt::ApplicationState state);
    void at_videowallControlMessageReceived(const ec2::ApiVideowallControlMessageData& apiMessage);

    void suspend();
    void resume();

    void doConnect();
    void doDisconnect(bool force = false);

    void updateConnectionState();


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

    connect(qnClientMessageProcessor, &QnClientMessageProcessor::videowallControlMessageReceived,
            d, &QnConnectionManagerPrivate::at_videowallControlMessageReceived);
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

int QnConnectionManager::defaultServerPort() const {
    return DEFAULT_APPSERVER_PORT;
}

QUrl connectToServerReplacementUrl;

void QnConnectionManager::connectToServer(const QUrl &url) {
    Q_D(QnConnectionManager);

    QUrl actualUrl = url;
    if (!connectToServerReplacementUrl.isEmpty())
    {
        actualUrl = connectToServerReplacementUrl;
        connectToServerReplacementUrl.clear();
        PRINT << "QnConnectionManager::connectToServer(): Using replacement url: "
            << actualUrl.toString();
    }
    else
    {
        PRINT << "QnConnectionManager::connectToServer(url: " << url.toString() << ")";
    }

    if (!actualUrl.isValid())
    {
        PRINT << "ERROR: connectToServer() cancelled because url is not valid: " << url.toString();
        return;
    }

    if (connectionState() != QnConnectionManager::Disconnected)
        disconnectFromServer(false);

    d->url = actualUrl;

    d->doConnect();
}

void QnConnectionManager::disconnectFromServer(bool force) {
    Q_D(QnConnectionManager);

    if (d->connectionHandle != kInvalidHandle)
    {
        d->connectionHandle = kInvalidHandle;
        d->url = QUrl();
        d->updateConnectionState();
        return;
    }

    if (d->connectionState == Disconnected || d->connectionState == Connecting)
        return;

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

void QnConnectionManagerPrivate::at_videowallControlMessageReceived(
    const ec2::ApiVideowallControlMessageData& apiMessage)
{
    if (apiMessage.instanceGuid !=
        QnRuntimeInfoManager::instance()->localInfo().data.videoWallInstanceGuid)
    {
        PRINT << "RECEIVED VideowallControlMessage for guid " << apiMessage.instanceGuid.toString()
            << "; ignored";
        return;
    }
    PRINT << "RECEIVED VideowallControlMessage, guid " << apiMessage.instanceGuid.toString();

    QnVideoWallControlMessage message;
    fromApiToResource(apiMessage, message);

    QnUuid controllerUuid = QnUuid(message[pcUuidKey]);
    qint64 sequence = message[sequenceKey].toULongLong();

    handleVideowallMessage(message, controllerUuid, sequence);
}

void QnConnectionManagerPrivate::handleVideowallMessage(
    const QnVideoWallControlMessage& message,
    const QnUuid& controllerUuid,
    qint64 sequence)
{
    Q_UNUSED(controllerUuid);
    Q_UNUSED(sequence);

    switch (static_cast<QnVideoWallControlMessage::Operation>(message.operation))
    {
        case QnVideoWallControlMessage::Exit:
        {
            PRINT << "RECEIVED VideowallControlMessage::Exit";
            QCoreApplication::exit();
            break;
        }

        case QnVideoWallControlMessage::EnterFullscreen:
        {
            PRINT << "RECEIVED VideowallControlMessage::EnterFullscreen";
            // TODO mike: Enter fullscreen.
            break;
        }

        case QnVideoWallControlMessage::ExitFullscreen:
        {
            PRINT << "RECEIVED VideowallControlMessage::ExitFullscreen";
            // TODO mike: Exit fullscreen.
            break;
        }

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

        connect(ec2Connection->getTimeManager().get(), &ec2::AbstractTimeManager::timeChanged, QnSyncTime::instance(), static_cast<void(QnSyncTime::*)(qint64)>(&QnSyncTime::updateTime));

        qnCommon->setLocalSystemName(connectionInfo.systemName);
        qnCommon->instance<QnUserWatcher>()->setUserName(url.userName());

        updateConnectionState();
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
