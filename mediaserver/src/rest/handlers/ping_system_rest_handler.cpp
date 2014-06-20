#include "ping_system_rest_handler.h"

#include "utils/network/simple_http_client.h"
#include "utils/network/tcp_connection_priv.h"

int QnPingSystemRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) {
    Q_UNUSED(path)

    QUrl url = params.value(lit("url"));
    QString password = params.value(lit("password"));

    if (!url.isValid()) {
        result.setError(QnJsonRestResult::InvalidParameter);
        return CODE_INVALID_PARAMETER;
    }

    if (password.isEmpty()) {
        result.setError(QnJsonRestResult::MissingParameter);
        return CODE_INVALID_PARAMETER;
    }

    QAuthenticator auth;
    auth.setUser(lit("admin"));
    auth.setPassword(password);

    CLSimpleHTTPClient client(url, 10000, auth);
    CLHttpStatus status = client.doGET(lit("api/moduleInformationAuthenticated"));

    if (status != CL_HTTP_SUCCESS) {
        result.setReply(QJsonValue(lit("FAIL")));
        return CODE_OK;
    }

    result.setReply(QJsonValue(lit("OK")));

    return CODE_OK;
}
