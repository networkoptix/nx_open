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

static const int kDefaultTimeoutMs = 10 * 1000;
static const QString kUrlParameter("url");
static const QString kKeyParameter("getKey");

int QnPingSystemRestHandler::executeGet(
        const QString& /*path*/,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner)
{
    nx::utils::Url url = params.value(kUrlParameter);
    QString getKey = params.value(kKeyParameter);

    if (url.isEmpty())
    {
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, kUrlParameter));
        return nx::network::http::StatusCode::ok;
    }

    if (!url.isValid())
    {
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::InvalidParameter, kUrlParameter));
        return nx::network::http::StatusCode::ok;
    }

    if (getKey.isEmpty())
    {
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, kKeyParameter));
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

    using namespace utils::MergeSystemsStatus;
    const Value mergeStatus = remoteLicensesConflict(owner->licensePool(), url, QAuthenticator());
    if (mergeStatus != Value::ok)
        result.setError(QnJsonRestResult::CantProcessRequest, toString(mergeStatus));

    return nx::network::http::StatusCode::ok;
}

nx::vms::api::ModuleInformation QnPingSystemRestHandler::remoteModuleInformation(
        const nx::utils::Url &url,
        const QString& getKey,
        int &status)
{
    CLSimpleHTTPClient client(url, kDefaultTimeoutMs, QAuthenticator());
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
