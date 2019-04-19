#include "exec_script_rest_handler.h"

#include <QProcess>
#include <QFileInfo>

#include <media_server/serverutil.h>
#include <nx/network/http/http_types.h>
#include <nx/network/rest/nx_network_rest_ini.h>
#include <nx/utils/file_system.h>

static const QString kAllowedParamChars = lit("^[a-zA-Z0-9_ =\\-\\+\\.]*$");

using namespace nx::network;

namespace nx::vms::server {

ExecScript::ExecScript(const QString& dataDirectory):
    m_dataDirectory(dataDirectory)
{
}

rest::Response ExecScript::executeGet(const rest::Request& request)
{
    if (!rest::ini().allowModificationsViaGetMethod)
        return nx::network::rest::Response::error(nx::network::rest::Result::Forbidden);

    return executePost(request);
}

rest::Response ExecScript::executePost(const rest::Request& request)
{
    if (request.path().contains(lit("..")) || request.path().contains('\\'))
        return rest::Response::error(rest::Result::Forbidden);

    QString scriptName = QFileInfo(request.path()).fileName();
    QString fileName = m_dataDirectory + "/scripts/" + scriptName;
    if (!QFile::exists(fileName))
    {
        return rest::Response::error(
            rest::Result::CantProcessRequest,
            lm("Script '%1' is missing at the server").arg(scriptName));
    }

    QStringList args;
    for (const auto& param: request.params().toList())
        args << lit("%1=%2").arg(param.first).arg(param.second);

    QRegExp regExpr(kAllowedParamChars);
    for (const auto& arg: args)
    {
        if (!regExpr.exactMatch(arg))
        {
            return rest::Response::error(
                rest::Result::CantProcessRequest,
                "Script params contain forbidden characters");
        }
    }

    return rest::Response::result(rest::JsonResult());
}

void ExecScript::afterExecute(const rest::Request& request, const rest::Response& /*response*/)
{
    QString scriptName = QFileInfo(request.path()).fileName();
    QString fileName = m_dataDirectory + "/scripts/" + scriptName;

    QStringList args;
    for (const auto& param: request.params().toList())
        args << lit("%1=%2").arg(param.first).arg(param.second);

    if (!QProcess::startDetached(fileName, args))
        qWarning() << lit( "Can't start script '%1' because of system error").arg(scriptName);
}

} // namespace nx::vms::server
