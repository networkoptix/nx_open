// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_license_manager.h"

#include "../detail/call_sync.h"

namespace ec2 {

ErrorCode AbstractLicenseManager::getLicensesSync(QnLicenseList* outLicenses)
{
    return detail::callSync(
        [&](auto handler)
        {
            getLicenses(std::move(handler));
        },
        outLicenses);
}

ErrorCode AbstractLicenseManager::addLicensesSync(const QnLicenseList& licenses)
{
    return detail::callSync(
        [&](auto handler)
        {
            addLicenses(licenses, std::move(handler));
        });
}

ErrorCode AbstractLicenseManager::removeLicenseSync(const QnLicensePtr& license)
{
    return detail::callSync(
        [&](auto handler)
        {
            removeLicense(license, std::move(handler));
        });
}

} // namespace ec2
