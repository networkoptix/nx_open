#include "remote_licenses.h"

#include <nx_ec/data/api_conversion_functions.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/deprecated/simple_http_client.h>
#include <nx/vms/api/data/license_data.h>

namespace {

static constexpr int kDefaultTimeoutMs = 10 * 1000;

} // namespace

QnLicenseList remoteLicenses(const nx::utils::Url& url, const QAuthenticator& auth, int* status)
{
    QnLicenseList result;

    CLSimpleHTTPClient client(url, kDefaultTimeoutMs, auth);
    CLHttpStatus requestStatus = client.doGET(QString("ec2/getLicenses"));
    if (status)
        *status = requestStatus;

    if (requestStatus != CL_HTTP_SUCCESS)
        return result;

    QByteArray data;
    client.readAll(data);

    const auto apiLicenses = QJson::deserialized<nx::vms::api::LicenseDataList>(data);
    ec2::fromApiToResourceList(apiLicenses, result);

    return result;
}

utils::MergeSystemsStatus::Value remoteLicensesConflict(
    QnLicensePool* localLicensesPool,
    const nx::utils::Url& url,
    const QAuthenticator& auth)
{
    using namespace utils::MergeSystemsStatus;

    // These license types are allowed to exist once per system and linked to corresponding error
    // codes.
    using UniqueLicenseType = std::pair<Qn::LicenseType, Value>;
    static const std::vector<UniqueLicenseType> kUniqueLicenseTypes{
        {Qn::LC_Start, Value::starterLicense},
        {Qn::LC_Nvr, Value::nvrLicense},
    };

    const auto localLicenses = QnLicenseListHelper(localLicensesPool->getLicenses());
    std::optional<QnLicenseListHelper> remoteLicensesList;
    for (const auto& [licenseType, errorCode]: kUniqueLicenseTypes)
    {
        const bool localSystemHasLicenses =
            localLicenses.totalLicenseByType(licenseType, localLicensesPool->validator()) > 0;

        // If we have no limited licenses in the local system, that's ok.
        if (!localSystemHasLicenses)
            continue;

        // Request remote licenses only once and only if needed.
        if (!remoteLicensesList)
             remoteLicensesList = QnLicenseListHelper(remoteLicenses(url, auth));

        const bool hasConflict = remoteLicensesList->totalLicenseByType(licenseType, nullptr) > 0;
        if (hasConflict)
            return errorCode;
    }
    return Value::ok;
}