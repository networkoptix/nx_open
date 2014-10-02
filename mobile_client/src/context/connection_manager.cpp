#include "connection_manager.h"

#include <QtCore/QUrl>

#include "api/abstract_connection.h"
#include "api/app_server_connection.h"
#include "api/session_manager.h"
#include "api/model/connection_info.h"
#include "api/global_settings.h"
#include "mobile_client/mobile_client_message_processor.h"
#include "utils/common/app_info.h"
#include "mobile_client/mobile_client_settings.h"

#include "utils/network/simple_http_client.h"


QnConnectionManager::QnConnectionManager(QObject *parent) :
    QObject(parent),
    m_connected(false),
    m_connectHandle(-1)
{
}

bool QnConnectionManager::connected() const {
    return m_connected;
}

void QnConnectionManager::connect(const QUrl &url) {
    if (!url.isValid()) {
        return;
    }

    connectToServer(url);
}

void QnConnectionManager::disconnect(bool force) {    
    disconnectFromServer(force);
}

bool QnConnectionManager::connectToServer(const QUrl &url) {
    qDebug() << "requested connection to " << url << " user = " << url.userName() << " pass = " << url.password();

    QAuthenticator auth;
    auth.setUser(url.userName());
    auth.setPassword(url.password());
    CLSimpleHTTPClient client(url, 3000, auth);
    CLHttpStatus st = client.doGET(url.path());
    qDebug() << "================= made GET: " << st;
    if (st == CL_HTTP_SUCCESS) {
        QByteArray data;
        client.readAll(data);
        qDebug() << data;
    }

    return false;

    if (connected() && !disconnectFromServer(false))
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

    QnAppServerConnectionFactory::setUrl(url);
    QnAppServerConnectionFactory::setEc2Connection(result.connection());
    QnAppServerConnectionFactory::setCurrentVersion(connectionInfo.version);

    QnMobileClientMessageProcessor::instance()->init(QnAppServerConnectionFactory::getConnection2());

    QnSessionManager::instance()->start();
//    QnResource::startCommandProc();

    emit connected();
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
    emit disconnected();
    return true;
}
