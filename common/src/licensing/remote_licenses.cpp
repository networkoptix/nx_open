#include "remote_licenses.h"

#include <nx_ec/data/api_license_data.h>
#include <nx_ec/data/api_conversion_functions.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/simple_http_client.h>

namespace {
    const int defaultTimeoutMs = 10 * 1000;
}

QnLicenseList remoteLicenses(const nx::utils::Url &url, const QAuthenticator &auth, int *status) {
    QnLicenseList result;

    CLSimpleHTTPClient client(url, defaultTimeoutMs, auth);
    CLHttpStatus requestStatus = client.doGET(lit("ec2/getLicenses"));
    if (status)
        *status = requestStatus;

    if (requestStatus != CL_HTTP_SUCCESS)
        return result;

    QByteArray data;
    client.readAll(data);

    ec2::ApiLicenseDataList apiLicenses = QJson::deserialized<ec2::ApiLicenseDataList>(data);
    fromApiToResourceList(apiLicenses, result);

    return result;
}
