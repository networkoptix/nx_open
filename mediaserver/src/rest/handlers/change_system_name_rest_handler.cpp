#include "change_system_name_rest_handler.h"

#include <nx_ec/ec_api.h>
#include <nx_ec/dummy_handler.h>
#include <api/app_server_connection.h>
#include <common/common_module.h>
#include <media_server/settings.h>

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

    bool wholeSystem = !params.value("wholeSystem").isEmpty();

    if (wholeSystem)
        QnAppServerConnectionFactory::getConnection2()->getMiscManager()->changeSystemName(systemName, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    else
        MSSettings::roSettings()->setValue("systemName", systemName);

    return HttpStatusCode::ok;
}

int QnChangeSystemNameRestHandler::executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, QByteArray &result, QByteArray &contentType) {
    Q_UNUSED(body)

    return executeGet(path, params, result, contentType);
}

QString QnChangeSystemNameRestHandler::description() const {
    return
        "Changes system name of this server<br>"
        "Request format: GET /api/changeSystemname?systemName=<system name>&amp;reboot&amp;<br>";
}
