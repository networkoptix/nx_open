#include "change_system_name_rest_handler.h"

#include <common/common_module.h>
#include <media_server/settings.h>

void stopServer(int signal);

namespace HttpStatusCode {
    enum Code {
        ok = 200
    };
}

int QnChangeSystemNameRestHandler::executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType) {
    Q_UNUSED(path)
    Q_UNUSED(contentType)
    Q_UNUSED(result)

    QString systemName = params.value("systemName");
    if (qnCommon->localSystemName() == systemName)
        return HttpStatusCode::ok;

    bool reboot = params.contains("reboot");

    MSSettings::roSettings()->setValue("systemName", systemName);

    if (reboot)
        stopServer(0);

    return HttpStatusCode::ok;
}

int QnChangeSystemNameRestHandler::executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, QByteArray &result, QByteArray &contentType) {
    Q_UNUSED(body)

    return executeGet(path, params, result, contentType);
}

QString QnChangeSystemNameRestHandler::description() const {
    return
        "Changes system name of this server<br>"
        "Request format: GET /api/changeSystemname?systemName=<system name>&amp;reboot&amp;<br>"
        "'reboot' parameter is optional.<br>";
}
