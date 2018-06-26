#include "restore_state_rest_handler.h"

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/serverutil.h>
#include <media_server/settings.h>
#include <media_server_process.h>
#include <nx/network/http/http_types.h>
#include <nx/vms/utils/vms_utils.h>
#include <rest/helpers/permissions_helper.h>
#include <rest/server/rest_connection_processor.h>
#include <rest/server/rest_connection_processor.h>
#include <server/server_globals.h>

int QnRestoreStateRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& params,
    const QByteArray& /*body*/,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    const Qn::UserAccessData& accessRights = owner->accessRights();

    if (QnPermissionsHelper::isSafeMode(owner->commonModule()))
        return QnPermissionsHelper::safeModeError(result);
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), accessRights))
        return QnPermissionsHelper::notOwnerError(result);

    if (QnPermissionsHelper::isSafeMode(owner->commonModule()))
        return QnPermissionsHelper::safeModeError(result);

    if (!verifyCurrentPassword(params, owner, &result))
        return nx::network::http::StatusCode::ok;

    return nx::network::http::StatusCode::ok;
}

void QnRestoreStateRestHandler::afterExecute(
    const QString& /*path*/,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QnRestConnectionProcessor* owner)
{
    if (!verifyCurrentPassword(params.toHash(), owner))
        return;

    QnJsonRestResult reply;
    if (QJson::deserialize(body, &reply) && reply.error == QnJsonRestResult::NoError)
    {
        qnServerModule->mutableSettings()->removeDbOnStartup.set(true);
        restartServer(0);
    }
}

static const auto kCurrentPasswordParamName = lit("currentPassword");

bool QnRestoreStateRestHandler::verifyCurrentPassword(
    const QnRequestParams& params,
    const QnRestConnectionProcessor* owner,
    QnJsonRestResult* result)
{
    const auto user = owner->commonModule()->resourcePool()
        ->getResourceById<QnUserResource>(owner->accessRights().userId);

    if (!user)
    {
        const auto error = lit(
            "User is not available, this handler is supposed to be used with authorization only");

        NX_ASSERT(false, error);
        result->setError(QnJsonRestResult::CantProcessRequest, error);
        return false;
    }

    if (user->checkLocalUserPassword(params.value(kCurrentPasswordParamName)))
        return true;

    if (result)
    {
        result->setError(QnJsonRestResult::CantProcessRequest,
            lit("Mandatory parameter '%1' is invalid").arg(kCurrentPasswordParamName));
    }

    return false;
}
