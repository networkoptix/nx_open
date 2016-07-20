#include "restore_state_rest_handler.h"

#include <nx/network/http/httptypes.h>
#include <rest/server/rest_connection_processor.h>
#include <media_server/serverutil.h>
#include <media_server/settings.h>
#include <server/server_globals.h>
#include <media_server_process.h>

int QnRestoreStateRestHandler::executeGet(
    const QString& path,
    const QnRequestParams & params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    return execute(std::move(PasswordData(params)), owner->authUserId(), result);
}

int QnRestoreStateRestHandler::executePost(
    const QString &path,
    const QnRequestParams &params,
    const QByteArray &body,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    Q_UNUSED(params);

    PasswordData passwordData = QJson::deserialized<PasswordData>(body);
    return execute(std::move(passwordData), owner->authUserId(), result);
}

int QnRestoreStateRestHandler::execute(PasswordData passwordData, const QnUuid &userId, QnJsonRestResult &result)
{
    QString errStr;
    if (!validatePasswordData(passwordData, &errStr) ||
        !validateAdminPassword(passwordData, &errStr))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, errStr);
        return nx_http::StatusCode::ok;
    }

    return nx_http::StatusCode::ok;
}

void QnRestoreStateRestHandler::afterExecute(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    Q_UNUSED(params);
    Q_UNUSED(owner);

    QnJsonRestResult reply;
    if (QJson::deserialize(body, &reply) && reply.error == QnJsonRestResult::NoError)
    {
        MSSettings::roSettings()->setValue(QnServer::kRemoveDbParamName, "1");
        nx::SystemName systemName;
        systemName.resetToDefault();
        systemName.saveToConfig();
        restartServer(0);
    }
}
