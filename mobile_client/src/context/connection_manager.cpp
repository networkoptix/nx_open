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

QnConnectionManager::QnConnectionManager(QObject *parent) :
    QObject(parent),
    m_connected(false),
    m_connectHandle(-1)
{
}

bool QnConnectionManager::isConnected() const {
    return m_connected;
}

bool QnConnectionManager::connectToServer(const QUrl &url) {
    if (!url.isValid()) {
        return true;
    }

    if (isConnected() && !disconnectFromServer(false))
        return true;

    QnEc2ConnectionRequestResult result;
    m_connectHandle = QnAppServerConnectionFactory::ec2ConnectionFactory()->connect(url, &result, &QnEc2ConnectionRequestResult::processEc2Reply);

    ec2::ErrorCode errorCode = static_cast<ec2::ErrorCode>(result.exec());
    if (m_connectHandle != result.handle())
        return true;

    m_connectHandle = -1;

    QnConnectionInfo connectionInfo = result.reply<QnConnectionInfo>();
    ConnectionStatus status = Success;

    if (errorCode == ec2::ErrorCode::unauthorized) {
        status = Unauthorized;
        qDebug() << "authorization failed";
    } else if (errorCode != ec2::ErrorCode::ok) {
        status = NetworkError;
        qDebug() << "unknown error, errorCode = " << static_cast<int>(errorCode);
    } else if (!connectionInfo.brand.isEmpty() && connectionInfo.brand != QnAppInfo::productNameShort()) {
        status = InvalidServer;
        qDebug() << "connecting to incompatible server: " << connectionInfo.brand;
    } else if (!isCompatible(connectionInfo.version, QnSoftwareVersion(QnAppInfo::applicationVersion()))) {
        status = InvalidVersion;
        qDebug() << "connecting to incompatible server: " << connectionInfo.version.toString() << " current: " << QnAppInfo::applicationVersion();
    }

    if (status != Success) {
        emit connectionFailed(url, status);
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

    emit connected(url);
//    context()->setUserName(appServerUrl.userName());
    return true;
}

bool QnConnectionManager::disconnectFromServer(bool force) {
    if (!force) {
        QnGlobalSettings::instance()->synchronizeNow();
//        qnSettings->setLastUsedConnection(QnConnectionData());
    }

    QnMobileClientMessageProcessor::instance()->init(NULL);
    QnAppServerConnectionFactory::setUrl(QUrl());
    QnAppServerConnectionFactory::setEc2Connection(NULL);
    QnAppServerConnectionFactory::setCurrentVersion(QnSoftwareVersion());

    QnSessionManager::instance()->stop();
//    QnResource::stopCommandProc();

//    context()->setUserName(QString());
    m_connected = false;
    emit disconnected();
    return true;
}
