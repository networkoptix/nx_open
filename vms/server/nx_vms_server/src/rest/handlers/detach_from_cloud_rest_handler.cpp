#include "detach_from_cloud_rest_handler.h"

#include <nx/network/http/http_types.h>
#include <nx/utils/log/log.h>

#include <nx/vms/cloud_integration/cloud_manager_group.h>
#include <nx/vms/cloud_integration/vms_cloud_connection_processor.h>

#include <api/global_settings.h>
#include <api/model/detach_from_cloud_data.h>
#include <api/model/detach_from_cloud_reply.h>
#include <common/common_module.h>
#include <network/system_helpers.h>
#include <rest/server/rest_connection_processor.h>
#include <rest/helpers/permissions_helper.h>

#include "private/multiserver_request_helper.h"

QnDetachFromCloudRestHandler::QnDetachFromCloudRestHandler(
    QnMediaServerModule* serverModule,
    nx::vms::cloud_integration::CloudManagerGroup* cloudManagerGroup)
    :
    QnJsonRestHandler(),
    nx::vms::server::ServerModuleAware(serverModule),
    m_cloudManagerGroup(cloudManagerGroup)
{
}

int QnDetachFromCloudRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    QnJsonRestResult& /*result*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    return nx::network::http::StatusCode::forbidden;
}

int QnDetachFromCloudRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    DetachFromCloudData passwordData = QJson::deserialized<DetachFromCloudData>(body);
    return execute(std::move(passwordData), owner, result);
}

int QnDetachFromCloudRestHandler::execute(
    DetachFromCloudData data,
    const QnRestConnectionProcessor* owner,
    QnJsonRestResult& result)
{
    if (!detail::verifyPasswordOrSetError(owner, data.currentPassword, &result))
        return nx::network::http::StatusCode::ok;

    const Qn::UserAccessData& accessRights = owner->accessRights();

    NX_VERBOSE(this, lm("Detaching system from cloud. cloudSystemId %1")
        .arg(owner->globalSettings()->cloudSystemId()));

    if (QnPermissionsHelper::isSafeMode(serverModule()))
    {
        NX_DEBUG(this, lm("Cannot detach from cloud while in safe mode. cloudSystemId %1")
            .arg(owner->globalSettings()->cloudSystemId()));
        return QnPermissionsHelper::safeModeError(result);
    }
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), accessRights))
    {
        NX_DEBUG(this, lm("Cannot detach from cloud. Owner permissions are required. cloudSystemId %1")
            .arg(owner->globalSettings()->cloudSystemId()));
        return QnPermissionsHelper::notOwnerError(result);
    }

    nx::vms::cloud_integration::VmsCloudConnectionProcessor vmsCloudConnectionProcessor(
        owner->commonModule(),
        m_cloudManagerGroup);
    DetachFromCloudReply reply;
    const auto resultCode = vmsCloudConnectionProcessor.detachFromCloud(data, &reply);
    if (resultCode != nx::network::http::StatusCode::ok)
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            vmsCloudConnectionProcessor.errorDescription());
        result.setReply(reply);
    }
    else
    {
        result.setError(QnJsonRestResult::NoError);
    }

    return resultCode;
}
