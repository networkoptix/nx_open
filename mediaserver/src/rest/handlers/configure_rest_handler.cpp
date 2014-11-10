#include "configure_rest_handler.h"

#include "nx_ec/ec_api.h"
#include "nx_ec/dummy_handler.h"
#include "transaction/transaction_message_bus.h"
#include "api/app_server_connection.h"
#include "common/common_module.h"
#include "media_server/settings.h"
#include "media_server/serverutil.h"
#include "media_server/server_connector.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/user_resource.h"
#include "core/resource/media_server_resource.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/network/module_finder.h"
#include "api/model/configure_reply.h"

void changePort(quint16 port);

namespace {
    enum Result {
        ResultOk,
        ResultFail,
        ResultSkip
    };

    ec2::AbstractECConnectionPtr ec2Connection() { return QnAppServerConnectionFactory::getConnection2(); }
}

int QnConfigureRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    Q_UNUSED(path)

    bool wholeSystem = params.value(lit("wholeSystem"), lit("false")) != lit("false");
    QString systemName = params.value(lit("systemName"));
    QString password = params.value(lit("password"));
    QString oldPassword = params.value(lit("oldPassword"));
    QByteArray passwordHash = params.value(lit("passwordHash")).toLatin1();
    QByteArray passwordDigest = params.value(lit("passwordDigest")).toLatin1();
    int port = params.value(lit("port")).toInt();

    /* set system name */
    int changeSystemNameResult = changeSystemName(systemName, wholeSystem);
    if (changeSystemNameResult == ResultFail) {
        result.setError(QnJsonRestResult::CantProcessRequest);
        result.setErrorString(lit("SYSTEM_NAME"));
    }

    /* reset connections if systemName is changed */
    if (changeSystemNameResult == ResultOk && !wholeSystem)
        resetConnections();

    /* set port */
    int changePortResult = changePort(port);
    if (changePortResult == ResultFail) {
        result.setError(QnJsonRestResult::CantProcessRequest);
        result.setErrorString(lit("PORT"));
    }

    /* set password */
    int changeAdminPasswordResult = changeAdminPassword(password, passwordHash, passwordDigest, oldPassword);
    if (changeAdminPasswordResult == ResultFail) {
        result.setError(QnJsonRestResult::CantProcessRequest);
        result.setErrorString(lit("PASSWORD"));
    }

    QnConfigureReply reply;
    reply.restartNeeded = false;
    result.setReply(reply);
    return CODE_OK;
}

int QnConfigureRestHandler::changeSystemName(const QString &systemName, bool wholeSystem) {
    if (systemName.isEmpty() || systemName == qnCommon->localSystemName())
        return ResultSkip;
    QnMediaServerResourcePtr server = qnResPool->getResourceById(qnCommon->moduleGUID()).dynamicCast<QnMediaServerResource>();
    if (!server)
        return ResultFail;

    if (!backupDatabase())
        return ResultFail;

    MSSettings::roSettings()->setValue("systemName", systemName);
    qnCommon->setLocalSystemName(systemName);

    server->setSystemName(systemName);
    ec2Connection()->getMediaServerManager()->save(server, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);

    if (wholeSystem)
        QnAppServerConnectionFactory::getConnection2()->getMiscManager()->changeSystemName(systemName, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);

    return ResultOk;
}

int QnConfigureRestHandler::changeAdminPassword(const QString &password, const QByteArray &passwordHash, const QByteArray &passwordDigest, const QString &oldPassword) {
    if (password.isEmpty() && (passwordHash.isEmpty() || passwordDigest.isEmpty()))
        return ResultSkip;

    QnUserResourcePtr admin = qnResPool->getAdministrator();
    if (!admin)
         return ResultFail;

    if (!password.isEmpty()) {
        /* check old password */
        if (!admin->checkPassword(oldPassword))
            return ResultFail;

        /* set new password */
        admin->setPassword(password);
        admin->generateHash();
        QnAppServerConnectionFactory::getConnection2()->getUserManager()->save(admin, this, [](int, ec2::ErrorCode) { return; });
        admin->setPassword(QString());
    } else {
        if (admin->getHash() != passwordHash || admin->getDigest() != passwordDigest) {
            admin->setHash(passwordHash);
            admin->setDigest(passwordDigest);
            QnAppServerConnectionFactory::getConnection2()->getUserManager()->save(admin, this, [](int, ec2::ErrorCode) { return; });
        }
    }
    return ResultOk;
}

int QnConfigureRestHandler::changePort(int port) {
    if (port == 0 || port == MSSettings::roSettings()->value(nx_ms_conf::SERVER_PORT).toInt())
        return ResultSkip;

    if (port < 0)
        return ResultFail;

    QnMediaServerResourcePtr server = qnResPool->getResourceById(qnCommon->moduleGUID()).dynamicCast<QnMediaServerResource>();
    if (!server)
        return ResultFail;

    {
        QAbstractSocket socket(QAbstractSocket::TcpSocket, 0);
        QAbstractSocket::BindMode bindMode = QAbstractSocket::DontShareAddress;
#ifndef Q_OS_WIN
        bindMode |= QAbstractSocket::ReuseAddressHint;
#endif
        if (!socket.bind(port, bindMode))
            return ResultFail;
    }
    ::changePort(port);

    MSSettings::roSettings()->setValue(nx_ms_conf::SERVER_PORT, port);

    return ResultOk;
}

void QnConfigureRestHandler::resetConnections() {
    if (qnTransactionBus) {
        qDebug() << "Dropping connections";
        qnTransactionBus->dropConnections();
    }

    if (QnServerConnector::instance())
        QnServerConnector::instance()->reconnect();
}
