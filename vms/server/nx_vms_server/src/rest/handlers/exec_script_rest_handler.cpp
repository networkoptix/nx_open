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

QnExecScript::QnExecScript(const QString& dataDirectory): m_dataDirectory(dataDirectory)
{
}

int QnExecScript::executeGet(
    const QString &path, const QnRequestParams &params, QnJsonRestResult &result,
    const QnRestConnectionProcessor* /*owner*/)
{
    if (path.contains(lit("..")) || path.contains('\\')) {
        result.setError(QnRestResult::InvalidParameter, "Path contains forbidden characters");
        return nx::network::http::StatusCode::ok;
    }

    QString scriptName = QFileInfo(path).fileName();
    QString fileName = m_dataDirectory + "/scripts/" + scriptName;
    if (!QFile::exists(fileName)) {
        result.setError(QnRestResult::InvalidParameter, lit( "Script '%1' is missing at the server").arg(scriptName));
        return nx::network::http::StatusCode::ok;
    }

    QStringList args;
    for (auto itr = params.begin(); itr != params.end(); ++itr)
        args << lit("%1=%2").arg(itr.key()).arg(itr.value());

    QRegExp regExpr(allowedParamChars);
    for (const auto& arg: args)
    {
        if (!regExpr.exactMatch(arg)) {
            result.setError(QnRestResult::InvalidParameter, "Script params contain forbidden characters");
            return nx::network::http::StatusCode::ok;
        }
    }

    return nx::network::http::StatusCode::ok;
}

int QnExecScript::executePost(
    const QString& path, const QnRequestParams& params, const QByteArray& /*body*/,
    QnJsonRestResult& result, const QnRestConnectionProcessor* owner)
{
    return executeGet(path, params, result, owner);
}

void QnExecScript::afterExecute(const QString& path, const QnRequestParamList& params,
    const QByteArray& body, const QnRestConnectionProcessor*)
{
    QnJsonRestResult reply;
    if (!QJson::deserialize(body, &reply) || reply.error !=  QnJsonRestResult::NoError)
        return;

    QString scriptName = QFileInfo(path).fileName();
    QString fileName = m_dataDirectory + "/scripts/" + scriptName;

    QStringList args;
    for (const auto& param: params)
        args << lit("%1=%2").arg(param.first).arg(param.second);

    if (!QProcess::startDetached(fileName, args))
        qWarning() << lit( "Can't start script '%1' because of system error").arg(scriptName);
}
