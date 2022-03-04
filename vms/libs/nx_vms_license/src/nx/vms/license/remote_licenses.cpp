// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_licenses.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/license/list_helper.h>
#include <nx/vms/license/validator.h>
#include <nx_ec/data/api_conversion_functions.h>

namespace {

// These license types are allowed to exist once per system and linked to corresponding
// merge system statuses.
using UniqueLicenseType = std::pair<Qn::LicenseType, MergeSystemsStatus>;
static const std::vector<UniqueLicenseType> kUniqueLicenseTypes{
    {Qn::LC_Start, MergeSystemsStatus::starterLicense},
    {Qn::LC_Nvr, MergeSystemsStatus::nvrLicense},
};

} // namespace

namespace nx::vms::license {

QnLicenseList remoteLicenses(
    const nx::utils::Url& url,
    const QAuthenticator& auth,
    nx::network::ssl::AdapterFunc adapterFunc,
    int* status)
{
    QnLicenseList result;

    nx::network::http::HttpClient httpClient{std::move(adapterFunc)};
    httpClient.setCredentials(auth);
    httpClient.setAllTimeouts(std::chrono::seconds(10));

    httpClient.doGet(nx::network::url::Builder(url).setPath("/ec2/getLicenses"));

    const auto statusCode = httpClient.statusCode();
    if (status)
        *status = statusCode;

    if (statusCode == nx::network::http::StatusCode::ok)
    {
        if (const auto response = httpClient.fetchEntireMessageBody())
        {
            const QByteArray data = response->toByteArray();

            const auto apiLicenses = QJson::deserialized<nx::vms::api::LicenseDataList>(data);
            ec2::fromApiToResourceList(apiLicenses, result);
        }
    }

    return result;
}

bool hasUniqueLicenses(QnLicensePool* localLicensesPool)
{
    const auto localLicenses = ListHelper(localLicensesPool->getLicenses());
    Validator validator(localLicensesPool->context());

    for (const auto& [licenseType, errorCode]: kUniqueLicenseTypes)
    {
        if (localLicenses.totalLicenseByType(licenseType, &validator) > 0)
            return true;
    }
    return false;
}

MergeSystemsStatus remoteLicensesConflict(
    QnLicensePool* localLicensesPool,
    const QnLicenseList& remoteLicenseList)
{
    const auto localLicenses = ListHelper(localLicensesPool->getLicenses());
    const auto remoteLicenses = ListHelper(remoteLicenseList);
    Validator validator(localLicensesPool->context());

    for (const auto& [licenseType, errorCode]: kUniqueLicenseTypes)
    {
        // If we have no limited licenses in the local system, that's ok.
        if ((localLicenses.totalLicenseByType(licenseType, &validator) > 0)
            && (remoteLicenses.totalLicenseByType(licenseType, nullptr) > 0))
        {
            return errorCode;
        }
    }

    return MergeSystemsStatus::ok;
}

MergeSystemsStatus remoteLicensesConflict(
    QnLicensePool* localLicensesPool,
    const nx::utils::Url& url,
    const QAuthenticator& auth,
    nx::network::ssl::AdapterFunc adapterFunc)
{
    const auto localLicenses = ListHelper(localLicensesPool->getLicenses());
    std::optional<ListHelper> remoteLicensesList;
    Validator validator(localLicensesPool->context());
    for (const auto& [licenseType, errorCode]: kUniqueLicenseTypes)
    {
        const bool localSystemHasLicenses =
            localLicenses.totalLicenseByType(licenseType, &validator) > 0;

        // If we have no limited licenses in the local system, that's ok.
        if (!localSystemHasLicenses)
            continue;

        // Request remote licenses only once and only if needed.
        if (!remoteLicensesList)
             remoteLicensesList = ListHelper(remoteLicenses(url, auth, std::move(adapterFunc)));

        const bool hasConflict = remoteLicensesList->totalLicenseByType(licenseType, nullptr) > 0;
        if (hasConflict)
            return errorCode;
    }
    return MergeSystemsStatus::ok;
}

} // namespace nx::vms::license
