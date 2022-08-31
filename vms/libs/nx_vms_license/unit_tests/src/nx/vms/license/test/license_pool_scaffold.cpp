// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "license_pool_scaffold.h"

#include "license_stub.h"

namespace nx::vms::license::test {

QnLicensePoolScaffold::QnLicensePoolScaffold(QnLicensePool* licensePool):
    m_licensePool(licensePool)
{
}

QnLicensePoolScaffold::~QnLicensePoolScaffold()
{
    m_licensePool->reset();
}

void QnLicensePoolScaffold::addLicenses(Qn::LicenseType licenseType, int count)
{
    auto stub = new QnLicenseStub(licenseType, count);
    m_licensePool->addLicense(QnLicensePtr(stub));
}

void QnLicensePoolScaffold::addLicense(Qn::LicenseType licenseType)
{
    addLicenses(licenseType, 1);
}

void QnLicensePoolScaffold::addFutureLicenses(int count)
{
    auto stub = new QnFutureLicenseStub(count);
    m_licensePool->addLicense(QnLicensePtr(stub));
}

} // namespace nx::vms::license::test
