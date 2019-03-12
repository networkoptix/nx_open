#include "restore_state_rest_handler.h"

#include <core/resource/user_resource.h>
#include <media_server/serverutil.h>
#include <media_server/settings.h>
#include <media_server_process.h>
#include <nx/network/http/http_types.h>
#include <nx/vms/utils/vms_utils.h>
#include <rest/helpers/permissions_helper.h>
#include <rest/server/rest_connection_processor.h>
#include <server/server_globals.h>

#include "private/multiserver_request_helper.h"

QnRestoreStateRestHandler::QnRestoreStateRestHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
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

    if (!detail::verifyPasswordOrSetError(owner, passwordData.currentPassword, &result))
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
        NX_INFO(this, "Server restart is scheduled");
        restartServer(0);
    }
}
