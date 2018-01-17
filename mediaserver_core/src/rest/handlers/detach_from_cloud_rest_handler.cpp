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

QnDetachFromCloudRestHandler::QnDetachFromCloudRestHandler(
    nx::vms::cloud_integration::CloudManagerGroup* cloudManagerGroup)
    :
    QnJsonRestHandler(),
    m_cloudManagerGroup(cloudManagerGroup)
{
}

int QnDetachFromCloudRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    return execute(std::move(DetachFromCloudData(params)), owner, result);
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
    const Qn::UserAccessData& accessRights = owner->accessRights();

    NX_LOGX(lm("Detaching system from cloud. cloudSystemId %1")
        .arg(owner->globalSettings()->cloudSystemId()), cl_logDEBUG2);

    if (QnPermissionsHelper::isSafeMode(owner->commonModule()))
    {
        NX_LOGX(lm("Cannot detach from cloud while in safe mode. cloudSystemId %1")
            .arg(owner->globalSettings()->cloudSystemId()), cl_logDEBUG1);
        return QnPermissionsHelper::safeModeError(result);
    }
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), accessRights))
    {
        NX_LOGX(lm("Cannot detach from cloud. Owner permissions are required. cloudSystemId %1")
            .arg(owner->globalSettings()->cloudSystemId()), cl_logDEBUG1);
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
