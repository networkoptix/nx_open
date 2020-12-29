#include "script_list_rest_handler.h"
#include <nx/network/http/http_types.h>
#include <QDir>
#include "media_server/serverutil.h"

QnScriptListRestHandler::QnScriptListRestHandler(const QString& dataDir):
    m_dataDir(dataDir)
{
}

int QnScriptListRestHandler::executeGet(const QString& /*path*/, const QnRequestParams& /*params*/,
    QnJsonRestResult& result, const QnRestConnectionProcessor*)
{
    QDir dir(m_dataDir + "/scripts");
    result.setReply(dir.entryList(QDir::NoDotAndDotDot | QDir::Files));

    return nx::network::http::StatusCode::ok;
}
