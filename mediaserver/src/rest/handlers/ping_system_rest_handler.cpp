#include "ping_system_rest_handler.h"

#include "common/common_module.h"
#include "utils/network/module_information.h"
#include "utils/network/simple_http_client.h"
#include "utils/network/tcp_connection_priv.h"
#include <utils/common/app_info.h>

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
        if (status == CL_HTTP_AUTH_REQUIRED)
            result.setErrorString(lit("UNAUTHORIZED"));
        else
            result.setErrorString(lit("FAIL"));
        return CODE_OK;
    }

    QByteArray data;
    client.readAll(data);

    QnJsonRestResult json;
    QJson::deserialize(data, &json);
    QnModuleInformation moduleInformation;
    QJson::deserialize(json.reply(), &moduleInformation);

    if (moduleInformation.systemName.isEmpty()) {
        /* Hmm there's no system name. It would be wrong system. Reject it. */
        result.setErrorString(lit("FAIL"));
        return CODE_OK;
    }

    if (moduleInformation.version != qnCommon->engineVersion() || moduleInformation.customization != QnAppInfo::customizationName()) {
        result.setErrorString(lit("INCOMPATIBLE"));
        return CODE_OK;
    }

    return CODE_OK;
}
