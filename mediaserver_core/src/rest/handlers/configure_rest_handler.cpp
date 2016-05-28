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
#include "network/tcp_connection_priv.h"
#include "network/module_finder.h"
#include "api/model/configure_reply.h"
#include "rest/server/rest_connection_processor.h"
#include "utils/crypt/linux_passwd_crypt.h"
#include "network/tcp_listener.h"
#include "server/host_system_password_synchronizer.h"
#include "audit/audit_manager.h"
#include "settings.h"


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

    if (MSSettings::roSettings()->value(nx_ms_conf::EC_DB_READ_ONLY).toInt()) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Can't change parameters because server is running in safe mode"));
        return nx_http::StatusCode::forbidden;
    }

    bool wholeSystem = params.value(lit("wholeSystem"), lit("false")) != lit("false");
    const QString systemName = params.value(lit("systemName"));
    const QString password = params.value(lit("password"));
    const QString oldPassword = params.value(lit("oldPassword"));

    const QByteArray realm = params.value(lit("realm")).toLatin1();
    const QByteArray passwordHash = params.value(lit("passwordHash")).toLatin1();
    const QByteArray passwordDigest = params.value(lit("passwordDigest")).toLatin1();
    QByteArray cryptSha512Hash = params.value(lit("cryptSha512Hash")).toLatin1();

    if (!(passwordHash.isEmpty() == realm.isEmpty() &&
          passwordDigest.isEmpty() == realm.isEmpty() &&
          cryptSha512Hash.isEmpty() == realm.isEmpty()))
    {
        //these values MUST be all filled or all NOT filled
        NX_LOG(lit("All password hashes MUST be supplied all together along with realm"), cl_logDEBUG2);
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lit("All password hashes MUST be supplied all together along with realm"));
        return CODE_OK;
    }

    qint64 sysIdTime = params.value(lit("sysIdTime")).toLongLong();
    qint64 tranLogTime = params.value(lit("tranLogTime")).toLongLong();
    int port = params.value(lit("port")).toInt();

    if( cryptSha512Hash.isEmpty() && !password.isEmpty() )
    {
        //genereating cryptSha512Hash
        cryptSha512Hash = linuxCryptSha512( password.toUtf8(), generateSalt( LINUX_CRYPT_SALT_LENGTH ) );
    }

    /* set system name */
    QString oldSystemName = qnCommon->localSystemName();
    int changeSystemNameResult = changeSystemName(systemName, sysIdTime, wholeSystem, tranLogTime);
    if (changeSystemNameResult == ResultFail) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("SYSTEM_NAME"));
    }

    /* reset connections if systemName is changed */
    if (changeSystemNameResult == ResultOk) {
        QnAuditRecord auditRecord = qnAuditManager->prepareRecord(owner->authSession(), Qn::AR_SystemNameChanged);
        QString description = lit("%1 -> %2").arg(oldSystemName).arg(systemName);
        auditRecord.addParam("description", description.toUtf8());
        qnAuditManager->addAuditRecord(auditRecord);

        if (!wholeSystem)
            resetConnections();
    }

    /* set port */
    int changePortResult = changePort(port);
    if (changePortResult == ResultFail) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Port is busy"));
        port = 0;   //not switching port
    }

    /* set password */
    int changeAdminPasswordResult = changeAdminPassword(password, realm, passwordHash, passwordDigest, cryptSha512Hash, oldPassword);
    if (changeAdminPasswordResult == ResultFail) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("PASSWORD"));
    }
    else {
        auto adminUser = qnResPool->getAdministrator();
        if (adminUser) {
            QnAuditRecord auditRecord = qnAuditManager->prepareRecord(owner->authSession(), Qn::AR_UserUpdate);
            auditRecord.resources.push_back(adminUser->getId());
            qnAuditManager->addAuditRecord(auditRecord);
        }
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

void QnConfigureRestHandler::afterExecute(const nx_http::Request& /*request*/, const QnRequestParamList& /*params*/,
                                          const QByteArray& /*body*/, const QnRestConnectionProcessor* /*owner*/)
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
    const QByteArray &realm,
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

    //making copy of admin user to be able to rollback local changed on DB update failure
    QnUserResourcePtr updateAdmin = QnUserResourcePtr(new QnUserResource(*admin));

    if (password.isEmpty() &&
        updateAdmin->getHash() == passwordHash &&
        updateAdmin->getDigest() == passwordDigest &&
        updateAdmin->getCryptSha512Hash() == cryptSha512Hash)
    {
        //no need to update anything
        return ResultOk;
    }

    if (!password.isEmpty()) {
        /* check old password */
        if (!admin->checkPassword(oldPassword))
            return ResultFail;

        /* set new password */
        updateAdmin->setPassword(password);
        updateAdmin->generateHash();
        QnUserResourceList updatedUsers;
        if (QnAppServerConnectionFactory::getConnection2()->getUserManager()->saveSync(updateAdmin, &updatedUsers) != ec2::ErrorCode::ok)
            return ResultFail;
        updateAdmin->setPassword(QString());
    } else {
        updateAdmin->setRealm(realm);
        updateAdmin->setHash(passwordHash);
        updateAdmin->setDigest(passwordDigest);
        if( !cryptSha512Hash.isEmpty() )
            updateAdmin->setCryptSha512Hash(cryptSha512Hash);
        QnUserResourceList updatedUsers;
        if (QnAppServerConnectionFactory::getConnection2()->getUserManager()->saveSync(updateAdmin, &updatedUsers) != ec2::ErrorCode::ok)
            return ResultFail;
    }

    //applying changes to local resource
    //TODO #ak following changes are done non-atomically
    admin->setRealm(updateAdmin->getRealm());
    admin->setHash(updateAdmin->getHash());
    admin->setDigest(updateAdmin->getDigest());
    admin->setCryptSha512Hash(updateAdmin->getCryptSha512Hash());

    HostSystemPasswordSynchronizer::instance()->syncLocalHostRootPasswordWithAdminIfNeeded(updateAdmin);
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
