#include "merge_systems_rest_handler.h"

#include <chrono>

#include <QtCore/QRegExp>

#include <audit/audit_manager.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <media_server/media_server_module.h>
#include <media_server/server_connector.h>
#include <media_server/serverutil.h>
#include <network/connection_validator.h>
#include <network/universal_tcp_listener.h>
#include <nx/utils/log/log.h>
#include <nx/vms/utils/system_merge_processor.h>
#include <nx/vms/utils/vms_utils.h>
#include <rest/helpers/permissions_helper.h>
#include <rest/server/rest_connection_processor.h>

QnMergeSystemsRestHandler::QnMergeSystemsRestHandler(QnMediaServerModule* serverModule):
    QnJsonRestHandler(),
    nx::vms::server::ServerModuleAware(serverModule)
{}

int QnMergeSystemsRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    return execute(std::move(MergeSystemData(params)), owner, result);
}

int QnMergeSystemsRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    MergeSystemData data = QJson::deserialized<MergeSystemData>(body);
    return execute(std::move(data), owner, result);
}

int QnMergeSystemsRestHandler::execute(
    MergeSystemData data,
    const QnRestConnectionProcessor* owner,
    QnJsonRestResult &result)
{
    // TODO: Require password at all times when we support required password in client, cloud and
    //     all tests, see VMS-12623.
    if (!data.currentPassword.isEmpty())
    {
        const auto authenticator = QnUniversalTcpListener::authenticator(owner->owner());
        if (!authenticator->isPasswordCorrect(owner->accessRights(), data.currentPassword))
        {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("Invalid current password"));
            return nx::network::http::StatusCode::ok;
        }
    }

    nx::vms::utils::SystemMergeProcessor systemMergeProcessor(owner->commonModule());
    using MergeStatus = ::utils::MergeSystemsStatus::Value;

    if (QnPermissionsHelper::isSafeMode(serverModule()))
    {
        systemMergeProcessor.setMergeError(&result, MergeStatus::safeMode);
    }
    else if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), owner->accessRights()))
    {
        systemMergeProcessor.setMergeError(&result, MergeStatus::forbidden);
        return nx::network::http::StatusCode::forbidden;
    }
    else
    {
        systemMergeProcessor.enableDbBackup(serverModule()->settings().backupDir());
        result = systemMergeProcessor.merge(
            owner->accessRights(),
            owner->authSession(),
            data);
    }

    if (result.error)
    {
        NX_DEBUG(this, lm("Merge with %1 failed with result %2")
            .args(data.url, toString(result.error)));
        return nx::network::http::StatusCode::ok;
    }

    updateLocalServerAuthKeyInConfig(owner->commonModule());

    initiateConnectionToRemoteServer(
        owner->commonModule(),
        QUrl(data.url),
        systemMergeProcessor.remoteModuleInformation());

    owner->commonModule()->auditManager()->addAuditRecord(
        owner->commonModule()->auditManager()->prepareRecord(owner->authSession(), Qn::AR_SystemmMerge));

    return nx::network::http::StatusCode::ok;
}

void QnMergeSystemsRestHandler::updateLocalServerAuthKeyInConfig(
    QnCommonModule* commonModule)
{
    QnMediaServerResourcePtr server =
        commonModule->resourcePool()->getResourceById<QnMediaServerResource>(
            commonModule->moduleGUID());
    NX_ASSERT(server);
    // TODO: #ak Following call better be made on event "current server data changed".
    nx::vms::server::SettingsHelper helper(serverModule());
    helper.setAuthKey(server->getAuthKey().toUtf8());
}

void QnMergeSystemsRestHandler::initiateConnectionToRemoteServer(
    QnCommonModule* commonModule,
    const QUrl& remoteModuleUrl,
    const nx::vms::api::ModuleInformationWithAddresses& remoteModuleInformation)
{
    nx::vms::discovery::ModuleEndpoint module(
        remoteModuleInformation,
        { remoteModuleUrl.host(), (uint16_t)remoteModuleInformation.port });
    commonModule->moduleDiscoveryManager()->checkEndpoint(module.endpoint, module.id);

    const auto connectionResult =
        QnConnectionValidator::validateConnection(remoteModuleInformation);

    /* Connect to server if it is compatible */
    if (connectionResult == Qn::SuccessConnectionResult && serverModule()->serverConnector())
        serverModule()->serverConnector()->addConnection(module);
}
