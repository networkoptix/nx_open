#include "detach_from_system_rest_handler.h"

#include <nx/cloud/cdb/api/connection.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/log/log.h>

#include <nx/vms/utils/vms_utils.h>

#include <api/global_settings.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <rest/server/rest_connection_processor.h>
#include <rest/helpers/permissions_helper.h>

#include <media_server/serverutil.h>
#include <nx/vms/cloud_integration/cloud_connection_manager.h>
#include <nx/vms/utils/detach_server_processor.h>
#include <utils/common/app_info.h>

QnDetachFromSystemRestHandler::QnDetachFromSystemRestHandler(
    QnMediaServerModule* serverModule,
    nx::vms::cloud_integration::CloudConnectionManager* const cloudConnectionManager,
    ec2::AbstractTransactionMessageBus* messageBus)
    :
    QnJsonRestHandler(),
    nx::mediaserver::ServerModuleAware(serverModule),
    m_cloudConnectionManager(cloudConnectionManager),
    m_messageBus(messageBus)
{
}

int QnDetachFromSystemRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    return execute(std::move(PasswordData(params)), owner, result);
}

int QnDetachFromSystemRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    PasswordData passwordData = QJson::deserialized<PasswordData>(body);
    return execute(std::move(passwordData), owner, result);
}

int QnDetachFromSystemRestHandler::execute(
    PasswordData data, const QnRestConnectionProcessor* owner, QnJsonRestResult& result)
{
    using namespace nx::cloud::db;
    const Qn::UserAccessData& accessRights = owner->accessRights();

    NX_DEBUG(this, lm("Detaching server from system started."));

    if (QnPermissionsHelper::isSafeMode(serverModule()))
    {
        NX_WARNING(this, lm("Cannot detach server from system while in safe mode."));
        return QnPermissionsHelper::safeModeError(result);
    }
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), accessRights))
    {
        NX_WARNING(this, lm("Cannot detach from system. Owner permissions are required."));
        return QnPermissionsHelper::notOwnerError(result);
    }

    QString errStr;
    if (!nx::vms::utils::validatePasswordData(data, &errStr))
    {
        NX_DEBUG(this, lm("Cannot detach from system. Password check failed.")
            );
        result.setError(QnJsonRestResult::CantProcessRequest, errStr);
        return nx::network::http::StatusCode::ok;
    }

    nx::mediaserver::Utils::dropConnectionsToRemotePeers(m_messageBus);

    nx::vms::utils::DetachServerProcessor detachServerProcessor(
        owner->commonModule(),
        m_cloudConnectionManager);
    const auto resultCode = detachServerProcessor.detachServer(&result);

    nx::mediaserver::Utils::resumeConnectionsToRemotePeers(m_messageBus);
    return resultCode;
}
