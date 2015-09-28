#include "script_list_rest_handler.h"
#include <utils/network/http/httptypes.h>
#include <QDir>
#include "media_server/serverutil.h"

int QnScriptListRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path);
    Q_UNUSED(params);

    QDir dir(getDataDirectory() + "/scripts");
    result.setReply(dir.entryList(QDir::NoDotAndDotDot | QDir::Files));

    return nx_http::StatusCode::ok;
}
