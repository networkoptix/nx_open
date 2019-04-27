#include "configure_rest_handler.h"

#include <QtCore/QSettings>

#include <api/global_settings.h>
#include <api/model/configure_reply.h>
#include <api/model/configure_system_data.h>
#include <audit/audit_manager.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <media_server/media_server_module.h>
#include <media_server/new_system_flag_watcher.h>
#include <media_server/serverutil.h>
#include <media_server/settings.h>
#include <network/tcp_listener.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/dummy_handler.h>
#include <nx_ec/ec_api.h>
#include <nx/network/http/http_types.h>
#include <nx/network/rest/nx_network_rest_ini.h>
#include <nx/utils/log/log.h>
#include <nx/vms/utils/system_merge_processor.h>
#include <nx/vms/utils/vms_utils.h>
#include <rest/helpers/permissions_helper.h>
#include <rest/server/rest_connection_processor.h>

#include "private/multiserver_request_helper.h"

namespace nx::vms::server {

enum Result
{
    ResultOk,
    ResultFail,
    ResultSkip
};

ConfigureRestHandler::ConfigureRestHandler(
    QnMediaServerModule* serverModule)
    :
    ServerModuleAware(serverModule)
{
}

nx::network::rest::Response ConfigureRestHandler::executeGet(
    const nx::network::rest::Request& request)
{
    if (!nx::network::rest::ini().allowModificationsViaGetMethod)
        return nx::network::rest::Response::error(nx::network::rest::Result::Forbidden);

    return executePost(request);
}

nx::network::rest::Response ConfigureRestHandler::executePost(
    const nx::network::rest::Request& request)
{
    auto data = request.parseContentOrThrow<ConfigureSystemData>();
    if (nx::network::rest::ini().allowModificationsViaGetMethod)
    {
        if (data.currentPassword.isEmpty())
            data.currentPassword = request.paramOrDefault("oldPassword");

        auto& log = data.tranLogTime;
        log.sequence = request.paramOrDefault<quint64>("tranLogTimeSequence", log.sequence);
        log.ticks = request.paramOrDefault<quint64>("tranLogTimeTicks", log.ticks);
    }

    nx::network::rest::JsonResult result;
    const auto status = execute(data, result, request.owner);
    auto response = nx::network::rest::Response::result(result);
    response.statusCode = status;
    return response;
}

// NOTE: Some parameters are undocumented and are used only by the server internally before calling
// /api/mergeSystem.
// TODO: Consider splitting into two methods: a public one (similar to the currently documented),
// and the proprietary one for the internal server use.
nx::network::http::StatusCode::Value ConfigureRestHandler::execute(
    const ConfigureSystemData& data,
    nx::network::rest::JsonResult& result,
    const QnRestConnectionProcessor* owner)
{
    nx::vms::server::Utils utils(serverModule());

    nx::vms::utils::SystemMergeProcessor systemMergeProcessor(owner->commonModule());
    using MergeStatus = ::utils::MergeSystemsStatus::Value;

    if (QnPermissionsHelper::isSafeMode(serverModule()))
    {
        systemMergeProcessor.setMergeError(&result, MergeStatus::safeMode);
        return nx::network::http::StatusCode::ok;
    }
    if (!QnPermissionsHelper::hasOwnerPermissions(
                 owner->resourcePool(), owner->accessRights()))
    {
        systemMergeProcessor.setMergeError(&result, MergeStatus::forbidden);
        return nx::network::http::StatusCode::forbidden;
    }

    if (QnPermissionsHelper::isSafeMode(serverModule()))
        return (nx::network::http::StatusCode::Value) QnPermissionsHelper::safeModeError(result);
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), owner->accessRights()))
        return (nx::network::http::StatusCode::Value) QnPermissionsHelper::notOwnerError(result);

    QString errStr;
    if (!nx::vms::utils::validatePasswordData(data, &errStr))
    {
        NX_WARNING(this, lit("%1").arg(errStr));
        result.setError(QnJsonRestResult::CantProcessRequest, errStr);
        return nx::network::http::StatusCode::ok;
    }

    if (data.hasPassword()
        && !detail::verifyPasswordOrSetError(owner, data.currentPassword, &result))
    {
        return nx::network::http::StatusCode::ok;
    }

    // Configure request must support systemName changes to maintain compatibility with NxTool
    if (!data.systemName.isEmpty())
    {
        owner->globalSettings()->setSystemName(data.systemName);
        owner->globalSettings()->synchronizeNowSync();
        // Update it now to prevent race condition. Normally it is updated with delay.
        serverModule()->findInstance<QnNewSystemServerFlagWatcher>()->update();
    }

    /* set system id and move tran log time */
    const auto oldSystemId = owner->globalSettings()->localSystemId();
    if (!data.localSystemId.isNull() && data.localSystemId != owner->globalSettings()->localSystemId())
    {
        NX_DEBUG(this, lm("Changing local system id from %1 to %2")
            .args(owner->globalSettings()->localSystemId(), data.localSystemId));

        if (!utils.backupDatabase())
        {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("SYSTEM_NAME"));
            NX_WARNING(this, lit("database backup error"));
            return nx::network::http::StatusCode::ok;
        }

        if (!utils.configureLocalSystem(data))
        {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("SYSTEM_NAME"));
            NX_WARNING(this, lit("can't change local system Id"));
            return nx::network::http::StatusCode::ok;
        }
        if (data.wholeSystem)
        {
            auto connection = owner->commonModule()->ec2Connection();
            auto manager = connection->getMiscManager(owner->accessRights());
            manager->changeSystemId(
                data.localSystemId,
                data.sysIdTime,
                data.tranLogTime,
                ec2::DummyHandler::instance(),
                &ec2::DummyHandler::onRequestDone);
        }
    }

    // rewrite system settings to update transaction time
    if (data.rewriteLocalSettings)
    {
        owner->commonModule()->ec2Connection()->setTransactionLogTime(data.tranLogTime);
        owner->globalSettings()->resynchronizeNowSync();
    }

    /* set port */
    int changePortResult = changePort(owner, data.port);
    if (changePortResult == ResultFail)
    {
        NX_WARNING(this, lit("can't change TCP port"));
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Port is busy"));
    }

    /* set password */
    if (data.hasPassword())
    {
        if (!utils.updateUserCredentials(
                data,
                QnOptionalBool(),
                owner->resourcePool()->getAdministrator()))
        {
            NX_WARNING(this, lit("can't update administrator credentials"));
            result.setError(QnJsonRestResult::CantProcessRequest, lit("PASSWORD"));
        }
        else
        {
            auto adminUser = owner->resourcePool()->getAdministrator();
            if (adminUser)
            {
                QnAuditRecord auditRecord = auditManager()->prepareRecord(owner->authSession(), Qn::AR_UserUpdate);
                auditRecord.resources.push_back(adminUser->getId());
                auditManager()->addAuditRecord(auditRecord);
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

    return nx::network::http::StatusCode::ok;
}

int ConfigureRestHandler::changePort(const QnRestConnectionProcessor* owner, int port)
{
    const Qn::UserAccessData& accessRights = owner->accessRights();

    int sPort = settings().port();
    if (port == 0 || port == sPort)
        return ResultSkip;

    if (port < 0)
        return ResultFail;

    QnMediaServerResourcePtr server =
        owner->resourcePool()->getResourceById<QnMediaServerResource>(owner->commonModule()->moduleGUID());
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

    nx::vms::api::MediaServerData apiServer;
    ec2::fromResourceToApi(server, apiServer);
    auto connection = owner->commonModule()->ec2Connection();
    auto manager = connection->getMediaServerManager(accessRights);
    auto errCode = manager->saveSync(apiServer);
    NX_ASSERT(errCode != ec2::ErrorCode::forbidden, "Access check should be implemented before");
    if (errCode != ec2::ErrorCode::ok)
        return ResultFail;

    serverModule()->mutableSettings()->port.set(port);
    return ResultOk;
}

} // namespace nx::vms::server
