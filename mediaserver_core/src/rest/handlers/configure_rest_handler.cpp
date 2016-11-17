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
#include <api/global_settings.h>
#include <api/resource_property_adaptor.h>

namespace
{
    enum Result
    {
        ResultOk,
        ResultFail,
        ResultSkip
    };

}

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
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->accessRights()))
        return QnPermissionsHelper::notOwnerError(result);

    QString errStr;
    if (!validatePasswordData(data, &errStr))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, errStr);
        return CODE_OK;
    }

    /* set system id and move tran log time */
    const auto oldSystemId = qnGlobalSettings->localSystemId();
    if (!data.localSystemId.isNull())
    {
        if (!backupDatabase())
        {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("SYSTEM_NAME"));
            return CODE_OK;
        }

        if (!changeLocalSystemId(data))
        {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("SYSTEM_NAME"));
            return CODE_OK;
        }
        if (data.wholeSystem)
        {
            auto connection = QnAppServerConnectionFactory::getConnection2();
            auto manager = connection->getMiscManager(owner->accessRights());
            manager->changeSystemId(
                data.localSystemId,
                data.sysIdTime,
                data.tranLogTime,
                ec2::DummyHandler::instance(),
                &ec2::DummyHandler::onRequestDone);
        }
        if (data.rewriteLocalSettings)
        {
            // rewrite system settings to update transaction time

            const auto& settings = QnGlobalSettings::instance()->allSettings();
            for (auto setting: settings)
                setting->markDirty();
            qnGlobalSettings->synchronizeNowSync();
        }
    }

    /* set port */
    int changePortResult = changePort(owner->accessRights(), data.port);
    if (changePortResult == ResultFail)
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Port is busy"));

    /* set password */
    if (data.hasPassword())
    {
        if (!updateUserCredentials(data, QnOptionalBool(), qnResPool->getAdministrator()))
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

int QnConfigureRestHandler::changePort(const Qn::UserAccessData& accessRights, int port)
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
    auto manager = connection->getMediaServerManager(accessRights);
    auto errCode = manager->saveSync(apiServer);
    NX_ASSERT(errCode != ec2::ErrorCode::forbidden, "Access check should be implemented before");
    if (errCode != ec2::ErrorCode::ok)
        return ResultFail;

    MSSettings::roSettings()->setValue(nx_ms_conf::SERVER_PORT, port);

    return ResultOk;
}
