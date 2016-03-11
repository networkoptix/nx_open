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
#include "network/tcp_listener.h"
#include "server/host_system_password_synchronizer.h"
#include "audit/audit_manager.h"
#include "settings.h"
#include <media_server/serverutil.h>


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
    
    PasswordData passwordData(params);

    QString errStr;
    if (!validatePasswordData(passwordData, &errStr))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, errStr);
        return CODE_OK;
    }

    qint64 sysIdTime = params.value(lit("sysIdTime")).toLongLong();
    qint64 tranLogTime = params.value(lit("tranLogTime")).toLongLong();
    int port = params.value(lit("port")).toInt();

    /* set system name */
    QString oldSystemName = qnCommon->localSystemName();
    if (!systemName.isEmpty() && systemName != qnCommon->localSystemName())
    {
        if (!backupDatabase())
        {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("SYSTEM_NAME"));
            return CODE_OK;
        }

        if (!changeSystemName(systemName, sysIdTime, tranLogTime))
        {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("SYSTEM_NAME"));
            return CODE_OK;
        }
        if (wholeSystem)
            QnAppServerConnectionFactory::getConnection2()->getMiscManager()->changeSystemName(systemName, sysIdTime, tranLogTime, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);

        /* reset connections if systemName is changed */
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
    if (passwordData.hasPassword())
    {
        if (!changeAdminPassword(passwordData)) {
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

void QnConfigureRestHandler::afterExecute(const QString& /*path*/, const QnRequestParamList& /*params*/,
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
