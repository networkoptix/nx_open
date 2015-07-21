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
#include "rest/server/rest_connection_processor.h"
#include "utils/crypt/linux_passwd_crypt.h"
#include "utils/network/tcp_listener.h"


namespace {
    enum Result {
        ResultOk,
        ResultFail,
        ResultSkip
    };
}

int QnConfigureRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor* owner) 
{
    Q_UNUSED(path)

    bool wholeSystem = params.value(lit("wholeSystem"), lit("false")) != lit("false");
    QString systemName = params.value(lit("systemName"));
    QString password = params.value(lit("password"));
    QString oldPassword = params.value(lit("oldPassword"));
    QByteArray passwordHash = params.value(lit("passwordHash")).toLatin1();
    QByteArray passwordDigest = params.value(lit("passwordDigest")).toLatin1();
    QByteArray cryptSha512Hash = params.value(lit("cryptSha512Hash")).toLatin1();
    qint64 sysIdTime = params.value(lit("sysIdTime")).toLongLong();
    qint64 tranLogTime = params.value(lit("tranLogTime")).toLongLong();
    int port = params.value(lit("port")).toInt();

    if( cryptSha512Hash.isEmpty() && !password.isEmpty() )
    {
        //genereating cryptSha512Hash
        cryptSha512Hash = linuxCryptSha512( password.toUtf8(), generateSalt( LINUX_CRYPT_SALT_LENGTH ) );
    }

    /* set system name */
    int changeSystemNameResult = changeSystemName(systemName, sysIdTime, wholeSystem, tranLogTime);
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
    int changeAdminPasswordResult = changeAdminPassword(password, passwordHash, passwordDigest, cryptSha512Hash, oldPassword);
    if (changeAdminPasswordResult == ResultFail) {
        result.setError(QnJsonRestResult::CantProcessRequest);
        result.setErrorString(lit("PASSWORD"));
    }

    QnConfigureReply reply;
    reply.restartNeeded = false;
    result.setReply(reply);

    if (port) {
        owner->owner()->updatePort(port);
        owner->owner()->waitForPortUpdated();
    }

    return CODE_OK;
}

void QnConfigureRestHandler::afterExecute(const QString &path, const QnRequestParamList &params, const QByteArray& body, const QnRestConnectionProcessor* owner)
{
    /*
    QnJsonRestResult reply;
    if (QJson::deserialize(body, &reply) && reply.error() ==  QnJsonRestResult::NoError) {
        int port = params.value(lit("port")).toInt();
        if (port) {
            owner->owner()->updatePort(port);
        }
    }
    */
}

int QnConfigureRestHandler::changeSystemName(const QString &systemName, qint64 sysIdTime, bool wholeSystem, qint64 remoteTranLogTime) {
    if (systemName.isEmpty() || systemName == qnCommon->localSystemName())
        return ResultSkip;

    if (!backupDatabase())
        return ResultFail;

    if (!::changeSystemName(systemName, sysIdTime, remoteTranLogTime))
        return ResultFail;

    if (wholeSystem)
        QnAppServerConnectionFactory::getConnection2()->getMiscManager()->changeSystemName(systemName, sysIdTime, remoteTranLogTime, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);

    return ResultOk;
}

int QnConfigureRestHandler::changeAdminPassword(
    const QString &password,
    const QByteArray &passwordHash,
    const QByteArray &passwordDigest,
    const QByteArray &cryptSha512Hash,
    const QString &oldPassword)
{
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
        if (admin->getHash() != passwordHash ||
            admin->getDigest() != passwordDigest ||
            admin->getCryptSha512Hash() != cryptSha512Hash)
        {
            admin->setHash(passwordHash);
            admin->setDigest(passwordDigest);
            if( !cryptSha512Hash.isEmpty() )
                admin->setCryptSha512Hash(cryptSha512Hash);
            QnAppServerConnectionFactory::getConnection2()->getUserManager()->save(admin, this, [](int, ec2::ErrorCode) { return; });
        }
    }
    return ResultOk;
}

int QnConfigureRestHandler::changePort(int port) {
    if (port == 0 || port == MSSettings::roSettings()->value(nx_ms_conf::SERVER_PORT, nx_ms_conf::DEFAULT_SERVER_PORT).toInt())
        return ResultSkip;

    if (port < 0)
        return ResultFail;

    QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
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
    
    QnMediaServerResourcePtr savedServer;
    QUrl url = server->getUrl();
    url.setPort(port);
    server->setUrl(url.toString());
    url = server->getApiUrl();
    url.setPort(port);
    server->setApiUrl(url.toString());
    if (QnAppServerConnectionFactory::getConnection2()->getMediaServerManager()->saveSync(server, &savedServer) != ec2::ErrorCode::ok)
        return ResultFail;


    MSSettings::roSettings()->setValue(nx_ms_conf::SERVER_PORT, port);

    return ResultOk;
}

void QnConfigureRestHandler::resetConnections() {
    if (QnServerConnector::instance())
        QnServerConnector::instance()->stop();

    qnTransactionBus->dropConnections();

    if (QnServerConnector::instance())
        QnServerConnector::instance()->start();
}
