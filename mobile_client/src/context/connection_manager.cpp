#include "connection_manager.h"

#include <QtCore/QUrl>

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

    const QnSoftwareVersion minimalSupportedVersion(2, 4, 0, 0);

}

QnConnectionManager::QnConnectionManager(QObject *parent) :
    QObject(parent)
{
    connect(qnCommon, &QnCommonModule::systemNameChanged, this, &QnConnectionManager::systemNameChanged);

    QnMobileClientMessageProcessor *messageProcessor = static_cast<QnMobileClientMessageProcessor*>(QnMobileClientMessageProcessor::instance());
    connect(messageProcessor,   &QnMobileClientMessageProcessor::connectionOpened,  this,   &QnConnectionManager::connectedChanged);
    connect(messageProcessor,   &QnMobileClientMessageProcessor::connectionClosed,  this,   &QnConnectionManager::connectedChanged);
}

bool QnConnectionManager::connected() const {
    QnMobileClientMessageProcessor *messageProcessor = static_cast<QnMobileClientMessageProcessor*>(QnMobileClientMessageProcessor::instance());
    return messageProcessor->isConnected();
}

QString QnConnectionManager::systemName() const {
    return qnCommon->localSystemName();
}

bool QnConnectionManager::connectToServer(const QUrl &url) {
    if (!url.isValid()) {
        return true;
    }

    if (connected() && !disconnectFromServer(false))
        return true;

    QnEc2ConnectionRequestResult result;
    int connectHandle = QnAppServerConnectionFactory::ec2ConnectionFactory()->connect(
        url, ec2::ApiClientInfoData(), &result, &QnEc2ConnectionRequestResult::processEc2Reply);

    ec2::ErrorCode errorCode = static_cast<ec2::ErrorCode>(result.exec());
    if (connectHandle != result.handle())
        return true;

    connectHandle = -1;

    QnConnectionInfo connectionInfo = result.reply<QnConnectionInfo>();
    ConnectionStatus status = Success;
    QString statusMessage;

    if (errorCode == ec2::ErrorCode::unauthorized) {
        status = Unauthorized;
        statusMessage = tr("Incorrect login or password");
    } else if (errorCode != ec2::ErrorCode::ok) {
        status = NetworkError;
        statusMessage = tr("Server or network is not available");
    } else if (!connectionInfo.brand.isEmpty() && connectionInfo.brand != QnAppInfo::productNameShort()) {
        status = InvalidServer;
        statusMessage = tr("Incompatible server");
    } else if (connectionInfo.version < minimalSupportedVersion) {
        status = InvalidVersion;
        statusMessage = tr("Incompatible server version %1").arg(connectionInfo.version.toString(QnSoftwareVersion::BugfixFormat));
    }

    if (status != Success) {
        qWarning() << "QnConnectionManager" << statusMessage;
        emit connectionFailed(status, statusMessage);
        return false;
    }

    ec2::AbstractECConnectionPtr ec2Connection = result.connection();

    QnAppServerConnectionFactory::setUrl(url);
    QnAppServerConnectionFactory::setEc2Connection(ec2Connection);
    QnAppServerConnectionFactory::setCurrentVersion(connectionInfo.version);

    QnMobileClientMessageProcessor::instance()->init(ec2Connection);

    QnSessionManager::instance()->start();
//    QnResource::startCommandProc();

    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoChanged, this, [this, ec2Connection](const QnPeerRuntimeInfo &info) {
        if (info.uuid != qnCommon->moduleGUID())
            return;
        ec2Connection->sendRuntimeData(info.data);
    });

    connect(ec2Connection->getTimeManager().get(), &ec2::AbstractTimeManager::timeChanged, QnSyncTime::instance(), static_cast<void(QnSyncTime::*)(qint64)>(&QnSyncTime::updateTime));

    qnCommon->setLocalSystemName(connectionInfo.systemName);
    qnCommon->instance<QnUserWatcher>()->setUserName(url.userName());

    connect(QnMobileClientMessageProcessor::instance(), &QnMobileClientMessageProcessor::initialResourcesReceived, this, &QnConnectionManager::initialResourcesReceived);

    return true;
}

bool QnConnectionManager::disconnectFromServer(bool force) {
    if (!force) {
        QnGlobalSettings::instance()->synchronizeNow();
    }

    QnMobileClientMessageProcessor::instance()->init(NULL);
    QnAppServerConnectionFactory::setUrl(QUrl());
    QnAppServerConnectionFactory::setEc2Connection(NULL);
    QnAppServerConnectionFactory::setCurrentVersion(QnSoftwareVersion());

    QnSessionManager::instance()->stop();
//    QnResource::stopCommandProc();

    qnCommon->instance<QnUserWatcher>()->setUserName(QString());
    return true;
}
