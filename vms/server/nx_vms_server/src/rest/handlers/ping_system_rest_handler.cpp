#include "ping_system_rest_handler.h"

#include "common/common_module.h"
#include <licensing/license.h>
#include <licensing/remote_licenses.h>

#include "nx/vms/discovery/manager.h"
#include "network/tcp_connection_priv.h"
#include <network/connection_validator.h>
#include "utils/common/app_info.h"
#include <nx/network/deprecated/simple_http_client.h>
#include <nx/network/http/custom_headers.h>
#include <rest/server/rest_connection_processor.h>
#include <licensing/license_validator.h>

//#define START_LICENSES_DEBUG

namespace
{
    const int defaultTimeoutMs = 10 * 1000;
}

int QnPingSystemRestHandler::executeGet(
        const QString& /*path*/,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner)
{
    nx::utils::Url url = params.value(lit("url"));
    QString getKey = params.value(lit("getKey"));

    if (url.isEmpty())
    {
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, lit("url")));
        return nx::network::http::StatusCode::ok;
    }

    if (!url.isValid())
    {
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::InvalidParameter, lit("url")));
        return nx::network::http::StatusCode::ok;
    }

    if (getKey.isEmpty())
    {
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, lit("password")));
        return nx::network::http::StatusCode::ok;
    }

    nx::vms::api::ModuleInformation moduleInformation;
    {
        int status = 0;
        moduleInformation = remoteModuleInformation(url, getKey, status);
        if (status != CL_HTTP_SUCCESS)
        {
            if (status == nx::network::http::StatusCode::unauthorized)
                result.setError(QnJsonRestResult::CantProcessRequest, lit("UNAUTHORIZED"));
            else if (status == nx::network::http::StatusCode::forbidden)
                result.setError(QnJsonRestResult::CantProcessRequest, lit("FORBIDDEN"));
            else
                result.setError(QnJsonRestResult::CantProcessRequest, lit("FAIL"));
            return nx::network::http::StatusCode::ok;
        }
    }

    if (moduleInformation.systemName.isEmpty())
    {
        /* Hmm there's no system name. It would be wrong system. Reject it. */
        result.setError(QnJsonRestResult::CantProcessRequest, lit("FAIL"));
        return nx::network::http::StatusCode::ok;
    }

    result.setReply(moduleInformation);

    auto connectionResult = QnConnectionValidator::validateConnection(moduleInformation);
    if (connectionResult != Qn::SuccessConnectionResult)
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("INCOMPATIBLE"));
        return nx::network::http::StatusCode::ok;
    }

    if (moduleInformation.ecDbReadOnly)
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("SAFE_MODE"));
        return nx::network::http::StatusCode::ok;
    }

    /* Check if there is a valid starter license in the local system. */
    QnLicenseListHelper helper(owner->licensePool()->getLicenses());
    if (helper.totalLicenseByType(Qn::LC_Start, owner->licensePool()->validator()) > 0)
    {

        /* Check if there is a valid starter license in the remote system. */
        QnLicenseList remoteLicensesList = remoteLicenses(url, QAuthenticator());

        /* Warn that some of the licenses will be deactivated. */
        QnLicenseListHelper remoteHelper(remoteLicensesList);
        if (remoteHelper.totalLicenseByType(Qn::LC_Start, nullptr) > 0)
            result.setError(QnJsonRestResult::CantProcessRequest, lit("STARTER_LICENSE_ERROR"));
    }

#ifdef START_LICENSES_DEBUG
    result.setError(QnJsonRestResult::CantProcessRequest, lit("STARTER_LICENSE_ERROR"));
#endif

    return nx::network::http::StatusCode::ok;
}

nx::vms::api::ModuleInformation QnPingSystemRestHandler::remoteModuleInformation(
        const nx::utils::Url &url,
        const QString& getKey,
        int &status)
{
    CLSimpleHTTPClient client(url, defaultTimeoutMs, QAuthenticator());
    status = client.doGET(
        lit("api/moduleInformationAuthenticated?checkOwnerPermissions=true&%1=%2").
        arg(QLatin1String(Qn::URL_QUERY_AUTH_KEY_NAME)).arg(getKey));

    if (status != CL_HTTP_SUCCESS)
        return {};

    QByteArray data;
    client.readAll(data);

    bool success = true;
    QnJsonRestResult json = QJson::deserialized<QnJsonRestResult>(data, QnJsonRestResult(), &success);
    return json.deserialized<nx::vms::api::ModuleInformation>();
}
