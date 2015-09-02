#include "ping_system_rest_handler.h"

#include "common/common_module.h"
#include <licensing/license.h>
#include <licensing/remote_licenses.h>

#include "utils/network/module_information.h"
#include "utils/network/simple_http_client.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/common/app_info.h"
#include "utils/network/module_finder.h"

//#define START_LICENSES_DEBUG

namespace {
    const int defaultTimeoutMs = 10 * 1000;
}

int QnPingSystemRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    Q_UNUSED(path)

    QUrl url = params.value(lit("url"));
    QString user = params.value(lit("user"), lit("admin"));
    QString password = params.value(lit("password"));

    if (url.isEmpty()) {
        result.setError(QnJsonRestResult::MissingParameter, lit("url"));
        return CODE_OK;
    }

    if (!url.isValid()) {
        result.setError(QnJsonRestResult::InvalidParameter, lit("url"));
        return CODE_OK;
    }

    if (password.isEmpty()) {
        result.setError(QnJsonRestResult::MissingParameter, lit("password"));
        return CODE_OK;
    }

    QAuthenticator auth;
    auth.setUser(user);
    auth.setPassword(password);

    QnModuleInformation moduleInformation;
    {
        int status;
        moduleInformation = remoteModuleInformation(url, auth, status);
        if (status != CL_HTTP_SUCCESS) {
            if (status == CL_HTTP_AUTH_REQUIRED)
                result.setError(QnJsonRestResult::CantProcessRequest, lit("UNAUTHORIZED"));
            else
                result.setError(QnJsonRestResult::CantProcessRequest, lit("FAIL"));
            return CODE_OK;
        }
    }
    

    if (moduleInformation.systemName.isEmpty()) {
        /* Hmm there's no system name. It would be wrong system. Reject it. */
        result.setError(QnJsonRestResult::CantProcessRequest, lit("FAIL"));
        return CODE_OK;
    }

    result.setReply(moduleInformation);

    bool customizationOK = moduleInformation.customization == QnAppInfo::customizationName() ||
                           moduleInformation.customization.isEmpty() ||
                           QnModuleFinder::instance()->isCompatibilityMode();
    if (!moduleInformation.hasCompatibleVersion() || !customizationOK) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("INCOMPATIBLE"));
        return CODE_OK;
    }

    /* Check if there is a valid starter license in the local system. */
    QnLicenseListHelper helper(qnLicensePool->getLicenses());
    if (helper.totalLicenseByType(Qn::LC_Start) > 0) {

        /* Check if there is a valid starter license in the remote system. */
        QnLicenseList remoteLicensesList = remoteLicenses(url, auth);

        /* Warn that some of the licenses will be deactivated. */
        QnLicenseListHelper remoteHelper(remoteLicensesList);
        if (remoteHelper.totalLicenseByType(Qn::LC_Start, true) > 0)
            result.setError(QnJsonRestResult::CantProcessRequest, lit("STARTER_LICENSE_ERROR"));
    }

#ifdef START_LICENSES_DEBUG
    result.setError(QnJsonRestResult::CantProcessRequest, lit("STARTER_LICENSE_ERROR"));
#endif

    return CODE_OK;
}

QnModuleInformation QnPingSystemRestHandler::remoteModuleInformation(const QUrl &url, const QAuthenticator &auth, int &status) {
    CLSimpleHTTPClient client(url, defaultTimeoutMs, auth);
    status = client.doGET(lit("api/moduleInformationAuthenticated"));

    if (status != CL_HTTP_SUCCESS)
        return QnModuleInformation();

    QByteArray data;
    client.readAll(data);

    bool success = true;
    QnJsonRestResult json = QJson::deserialized<QnJsonRestResult>(data, QnJsonRestResult(), &success);
    return json.deserialized<QnModuleInformation>();
}
