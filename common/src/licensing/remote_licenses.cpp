#include "remote_licenses.h"

#include <nx_ec/data/api_license_data.h>
#include <nx_ec/data/api_conversion_functions.h>

#include <utils/common/model_functions.h>
#include <utils/network/simple_http_client.h>

namespace {
    const int defaultTimeoutMs = 10 * 1000;
}

QnLicenseList remoteLicenses(const QUrl &url, const QAuthenticator &auth, int *status) {
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
