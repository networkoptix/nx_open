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

QnRestoreStateRestHandler::QnRestoreStateRestHandler(QnMediaServerModule* serverModule):
    nx::mediaserver::ServerModuleAware(serverModule)
{
}

int QnRestoreStateRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    auto passwordData = QJson::deserialized<CurrentPasswordData>(body);
    const Qn::UserAccessData& accessRights = owner->accessRights();

    if (QnPermissionsHelper::isSafeMode(serverModule()))
        return QnPermissionsHelper::safeModeError(result);
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), accessRights))
        return QnPermissionsHelper::notOwnerError(result);

    if (QnPermissionsHelper::isSafeMode(serverModule()))
        return QnPermissionsHelper::safeModeError(result);

    if (!verifyCurrentPassword(passwordData, owner, &result))
        return nx::network::http::StatusCode::ok;

    return nx::network::http::StatusCode::ok;
}

void QnRestoreStateRestHandler::afterExecute(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    const QByteArray& body,
    const QnRestConnectionProcessor* owner)
{
    auto result = QJson::deserialized<QnJsonRestResult>(body);
    if (result.error != QnRestResult::NoError)
        return;

    QnJsonRestResult reply;
    if (QJson::deserialize(body, &reply) && reply.error == QnJsonRestResult::NoError)
    {
        serverModule()->mutableSettings()->removeDbOnStartup.set(true);
        restartServer(0);
    }
}

bool QnRestoreStateRestHandler::verifyCurrentPassword(
    const CurrentPasswordData& passwordData,
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

    if (user->checkLocalUserPassword(passwordData.currentPassword))
        return true;

    if (result)
    {
        result->setError(QnJsonRestResult::CantProcessRequest,
            lit("Invalid current password provided"));
    }

    return false;
}
