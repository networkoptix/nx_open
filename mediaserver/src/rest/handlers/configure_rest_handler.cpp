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

namespace {
    enum Result {
        ResultOk,
        ResultFail,
        ResultSkip
    };

    ec2::AbstractECConnectionPtr ec2Connection() { return QnAppServerConnectionFactory::getConnection2(); }
}

int QnConfigureRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) {
    Q_UNUSED(path)

    bool wholeSystem = params.value(lit("wholeSystem"), lit("false")) == lit("true");
    QString systemName = params.value(lit("systemName"));
    QString password = params.value(lit("password"));
    QString oldPassword = params.value(lit("oldPassword"));
    QByteArray passwordHash = params.value(lit("passwordHash")).toLatin1();
    QByteArray passwordDigest = params.value(lit("passwordDigest")).toLatin1();
    int port = params.value(lit("port")).toInt();

    /* set system name */
    int changeSystemNameResult = changeSystemName(systemName, wholeSystem);
    if (changeSystemNameResult == ResultFail)
        result.setError(QnJsonRestResult::CantProcessRequest);

    /* reset connections if systemName is changed */
    if (changeSystemNameResult == ResultOk && !wholeSystem)
        resetConnections();

    /* set port */
    int changePortResult = changePort(port);
    if (changePortResult == ResultFail)
        result.setError(QnJsonRestResult::CantProcessRequest);

    /* set password */
    int changeAdminPasswordResult = changeAdminPassword(password, passwordHash, passwordDigest, oldPassword);
    if (changeAdminPasswordResult == ResultFail)
        result.setError(QnJsonRestResult::CantProcessRequest);

    QJsonObject res;
    res.insert(lit("restartNeeded"), changePortResult == ResultOk);
    result.setReply(QJsonValue(res));
    return result.error() == QnJsonRestResult::NoError ? CODE_OK : CODE_INTERNAL_ERROR;
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

    foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(Qn::user)) {
        QnUserResourcePtr user = resource.staticCast<QnUserResource>();
        if (user->getName() != lit("admin"))
            continue;

        if (!password.isEmpty()) {
            /* check old password */
            QList<QByteArray> values =  user->getHash().split(L'$');
            if (values.size() != 3)
                return ResultFail;

            QByteArray salt = values[1];
            QCryptographicHash md5(QCryptographicHash::Md5);
            md5.addData(salt);
            md5.addData(oldPassword.toUtf8());
            if (md5.result().toHex() != values[2])
                return ResultFail;

            /* set new password */
            user->setPassword(password);
            user->generateHash();
            QnAppServerConnectionFactory::getConnection2()->getUserManager()->save(user, this, [](int, ec2::ErrorCode) { return; });
            user->setPassword(QString());
        } else {
            if (user->getHash() != passwordHash || user->getDigest() != passwordDigest) {
                user->setHash(passwordHash);
                user->setDigest(passwordDigest);
                QnAppServerConnectionFactory::getConnection2()->getUserManager()->save(user, this, [](int, ec2::ErrorCode) { return; });
            }
        }
        return ResultOk;
    }
    return ResultFail;
}

int QnConfigureRestHandler::changePort(int port) {
    if (port == 0 || port == MSSettings::roSettings()->value(nx_ms_conf::SERVER_PORT).toInt())
        return ResultSkip;

    QnMediaServerResourcePtr server = qnResPool->getResourceById(qnCommon->moduleGUID()).dynamicCast<QnMediaServerResource>();
    if (!server)
        return ResultFail;

    MSSettings::roSettings()->setValue(nx_ms_conf::SERVER_PORT, port);

    //TODO: update port in TCP listener

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
