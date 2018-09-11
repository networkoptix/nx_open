#include "settings_documentation_handler.h"

#include <nx/mediaserver/settings.h>

QnSettingsDocumentationHandler::QnSettingsDocumentationHandler(
    const nx::mediaserver::Settings* settings):
    m_settings(settings)
{
}

int QnSettingsDocumentationHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* /*owner*/)
{
    result.setReply(m_settings->buildDocumentation());
    return nx::network::http::StatusCode::ok;
}
