#include "exec_script_rest_handler.h"
#include <nx/network/http/http_types.h>
#include <QFileInfo>
#include "media_server/serverutil.h"
#include <QProcess>
#include <nx/utils/file_system.h>

namespace
{
    const QString allowedParamChars = lit("^[a-zA-Z0-9_ =\\-\\+\\.]*$");
}

int QnExecScript::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    if (!nx::utils::file_system::isSafeRelativePath(path)) {
        result.setError(QnRestResult::InvalidParameter, "Path contains forbidden characters");
        return nx_http::StatusCode::ok;
    }

    QString scriptName = QFileInfo(path).fileName();
    QString fileName = getDataDirectory() + "/scripts/" + scriptName;
    if (!QFile::exists(fileName)) {
        result.setError(QnRestResult::InvalidParameter, lit( "Script '%1' is missing at the server").arg(scriptName));
        return nx_http::StatusCode::ok;
    }
    
    QStringList args;
    for (auto itr = params.begin(); itr != params.end(); ++itr)
        args << lit("%1=%2").arg(itr.key()).arg(itr.value());
    
    QRegExp regExpr(allowedParamChars);
    for (const auto& arg: args) 
    {
        if (!regExpr.exactMatch(arg)) {
            result.setError(QnRestResult::InvalidParameter, "Script params contain forbidden characters");
            return nx_http::StatusCode::ok;
        }
    }

    return nx_http::StatusCode::ok;
}

void QnExecScript::afterExecute(const QString &path, const QnRequestParamList &params, const QByteArray& body, const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(owner);
    QnJsonRestResult reply;
    if (!QJson::deserialize(body, &reply) || reply.error !=  QnJsonRestResult::NoError)
        return;

    QString scriptName = QFileInfo(path).fileName();
    QString fileName = getDataDirectory() + "/scripts/" + scriptName;

    QStringList args;
    for (const auto& param: params)
        args << lit("%1=%2").arg(param.first).arg(param.second);

    if (!QProcess::startDetached(fileName, args))
        qWarning() << lit( "Can't start script '%1' because of system error").arg(scriptName);
}
