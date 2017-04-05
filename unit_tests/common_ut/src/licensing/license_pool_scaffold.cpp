#include "license_pool_scaffold.h"

#include <licensing/license_stub.h>

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
    stub->setArmServer(m_arm);
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

void QnLicensePoolScaffold::setArmMode(bool isArm)
{
    m_arm = isArm;
}
