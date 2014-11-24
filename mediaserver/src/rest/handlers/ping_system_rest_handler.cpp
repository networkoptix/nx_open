#include "ping_system_rest_handler.h"

#include "common/common_module.h"
#include "utils/network/module_information.h"
#include "utils/network/simple_http_client.h"
#include "utils/network/tcp_connection_priv.h"
#include <utils/common/app_info.h>
#include "utils/network/module_finder.h"

int QnPingSystemRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    Q_UNUSED(path)

    QUrl url = params.value(lit("url"));
    QString user = params.value(lit("user"), lit("admin"));
    QString password = params.value(lit("password"));

    if (url.isEmpty()) {
        result.setError(QnJsonRestResult::MissingParameter);
        result.setErrorString(lit("url"));
        return CODE_OK;
    }

    if (!url.isValid()) {
        result.setError(QnJsonRestResult::InvalidParameter);
        result.setErrorString(lit("url"));
        return CODE_OK;
    }

    if (password.isEmpty()) {
        result.setError(QnJsonRestResult::MissingParameter);
        result.setErrorString(lit("password"));
        return CODE_OK;
    }

    QAuthenticator auth;
    auth.setUser(user);
    auth.setPassword(password);

    CLSimpleHTTPClient client(url, 10000, auth);
    CLHttpStatus status = client.doGET(lit("api/moduleInformationAuthenticated"));

    if (status != CL_HTTP_SUCCESS) {
        if (status == CL_HTTP_AUTH_REQUIRED)
            result.setError(QnJsonRestResult::CantProcessRequest, lit("UNAUTHORIZED"));
        else
            result.setError(QnJsonRestResult::CantProcessRequest, lit("FAIL"));
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
        result.setError(QnJsonRestResult::CantProcessRequest, lit("FAIL"));
        return CODE_OK;
    }

    result.setReply(moduleInformation);

    bool customizationOK = moduleInformation.customization == QnAppInfo::customizationName() ||
                           moduleInformation.customization.isEmpty() ||
                           QnModuleFinder::instance()->isCompatibilityMode();
    if (!isCompatible(qnCommon->engineVersion(), moduleInformation.version) || !customizationOK) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("INCOMPATIBLE"));
        return CODE_OK;
    }

    return CODE_OK;
}
