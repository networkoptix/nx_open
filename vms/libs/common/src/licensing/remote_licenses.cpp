#include "remote_licenses.h"

#include <nx_ec/data/api_conversion_functions.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/deprecated/simple_http_client.h>
#include <nx/vms/api/data/license_data.h>

using namespace nx;
using namespace nx::vms;

namespace {

static constexpr int kDefaultTimeoutMs = 10 * 1000;

} // namespace

QnLicenseList remoteLicenses(const utils::Url& url, const QAuthenticator& auth, int* status)
{
    QnLicenseList result;

    CLSimpleHTTPClient client(url, kDefaultTimeoutMs, auth);
    CLHttpStatus requestStatus = client.doGET(lit("ec2/getLicenses"));
    if (status)
        *status = requestStatus;

    if (requestStatus != CL_HTTP_SUCCESS)
        return result;

    QByteArray data;
    client.readAll(data);

    api::LicenseDataList apiLicenses = QJson::deserialized<api::LicenseDataList>(data);
    ec2::fromApiToResourceList(apiLicenses, result);

    return result;
}
