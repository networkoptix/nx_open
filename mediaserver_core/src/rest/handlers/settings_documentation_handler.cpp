#include "settings_documentation_handler.h"

#include "media_server/media_server_module.h"

int QnSettingsDocumentationHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* /*owner*/)
{
    result.setReply(qnServerModule->settings().buildDocumentation());
    return nx::network::http::StatusCode::ok;
}
