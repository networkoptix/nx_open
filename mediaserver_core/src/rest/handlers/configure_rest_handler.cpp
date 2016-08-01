#include "configure_rest_handler.h"

#include "nx_ec/ec_api.h"
#include "nx_ec/dummy_handler.h"
#include <nx_ec/data/api_user_data.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_user_manager.h>
#include <nx_ec/managers/abstract_server_manager.h>

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

#include <rest/helpers/permissions_helper.h>


namespace
{
    enum Result
    {
        ResultOk,
        ResultFail,
        ResultSkip
    };

}

struct ConfigureSystemData: public PasswordData
{
    ConfigureSystemData():
        PasswordData(),
        wholeSystem(false),
        sysIdTime(0),
        tranLogTime(0),
        port(0)
    {
    }

    ConfigureSystemData(const QnRequestParams& params):
        PasswordData(params),
        systemName(params.value(lit("systemName"))),
        wholeSystem(params.value(lit("wholeSystem"), lit("false")) != lit("false")),
        sysIdTime(params.value(lit("sysIdTime")).toLongLong()),
        tranLogTime(params.value(lit("tranLogTime")).toLongLong()),
        port(params.value(lit("port")).toInt())
    {
    }

    QString systemName;
    bool wholeSystem;
    qint64 sysIdTime;
    qint64 tranLogTime;
    int port;
};

#define ConfigureSystemData_Fields PasswordData_Fields (systemName)(wholeSystem)(sysIdTime)(tranLogTime)(port)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ConfigureSystemData),
    (json),
    _Fields,
    (optional, true));

int QnConfigureRestHandler::executeGet(
    const QString &path,
    const QnRequestParams &params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path)
    return execute(ConfigureSystemData(params), result, owner);
}

int QnConfigureRestHandler::executePost(
    const QString &path,
    const QnRequestParams &params,
    const QByteArray &body,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path)
    Q_UNUSED(params)
    ConfigureSystemData data = QJson::deserialized<ConfigureSystemData>(body);
    return execute(std::move(data), result, owner);
}

int QnConfigureRestHandler::execute(
    const ConfigureSystemData& data,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    if (QnPermissionsHelper::isSafeMode())
        return QnPermissionsHelper::safeModeError(result);
    if (!QnPermissionsHelper::isOwner(owner->authUserId()))
        return QnPermissionsHelper::notOwnerError(result);

    QString errStr;
    if (!validatePasswordData(data, &errStr))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, errStr);
        return CODE_OK;
    }

    /* set system name */
    QString oldSystemName = qnCommon->localSystemName();
    if (!data.systemName.isEmpty() && data.systemName != qnCommon->localSystemName())
    {
        if (!backupDatabase())
        {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("SYSTEM_NAME"));
            return CODE_OK;
        }

        if (!changeSystemName(
            data.systemName,
            data.sysIdTime,
            data.tranLogTime,
            !data.wholeSystem,
            Qn::UserAccessData(owner->authUserId())))
        {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("SYSTEM_NAME"));
            return CODE_OK;
        }
        if (data.wholeSystem)
        {
            auto connection = QnAppServerConnectionFactory::getConnection2();
            auto manager = connection->getMiscManager(Qn::UserAccessData(owner->authUserId()));
            manager->changeSystemName(
                data.systemName,
                data.sysIdTime,
                data.tranLogTime,
                ec2::DummyHandler::instance(),
                &ec2::DummyHandler::onRequestDone);
        }

        /* reset connections if systemName is changed */
        QnAuditRecord auditRecord = qnAuditManager->prepareRecord(owner->authSession(), Qn::AR_SystemNameChanged);
        QString description = lit("%1 -> %2").arg(oldSystemName).arg(data.systemName);
        auditRecord.addParam("description", description.toUtf8());
        qnAuditManager->addAuditRecord(auditRecord);
    }

    /* set port */
    int changePortResult = changePort(owner->authUserId(), data.port);
    if (changePortResult == ResultFail)
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Port is busy"));

    /* set password */
    if (data.hasPassword())
    {
        if (!changeAdminPassword(data, owner->authUserId()))
        {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("PASSWORD"));
        }
        else
        {
            auto adminUser = qnResPool->getAdministrator();
            if (adminUser)
            {
                QnAuditRecord auditRecord = qnAuditManager->prepareRecord(owner->authSession(), Qn::AR_UserUpdate);
                auditRecord.resources.push_back(adminUser->getId());
                qnAuditManager->addAuditRecord(auditRecord);
            }
        }
    }

    QnConfigureReply reply;
    reply.restartNeeded = false;
    result.setReply(reply);

    if (changePortResult == ResultOk)
    {
        owner->owner()->updatePort(data.port);
        owner->owner()->waitForPortUpdated();
    }

    return CODE_OK;
}

int QnConfigureRestHandler::changePort(const QnUuid &userId, int port)
{
    int sPort = MSSettings::roSettings()->value(
        nx_ms_conf::SERVER_PORT,
        nx_ms_conf::DEFAULT_SERVER_PORT).toInt();
    if (port == 0 || port == sPort)
        return ResultSkip;

    if (port < 0)
        return ResultFail;

    QnMediaServerResourcePtr server =
        qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
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

    QUrl url = server->getUrl();
    url.setPort(port);
    server->setUrl(url.toString());

    ec2::ApiMediaServerData apiServer;
    ec2::fromResourceToApi(server, apiServer);
    auto connection = QnAppServerConnectionFactory::getConnection2();
    auto manager = connection->getMediaServerManager(Qn::UserAccessData(userId));
    if (manager->saveSync(apiServer) != ec2::ErrorCode::ok)
        return ResultFail;

    MSSettings::roSettings()->setValue(nx_ms_conf::SERVER_PORT, port);

    return ResultOk;
}
