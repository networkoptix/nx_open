#include "license_pool_scaffold.h"

#include <licensing/license_stub.h>

QnLicensePoolScaffold::QnLicensePoolScaffold(bool isArm /* = false */):
    m_arm(isArm)
{
    licensePool();
}

QnLicensePoolScaffold::~QnLicensePoolScaffold() {
    licensePool()->reset();
}

void QnLicensePoolScaffold::addLicenses(Qn::LicenseType licenseType, int count) {
    auto stub = new QnLicenseStub(licenseType, count);
    stub->setArmServer(m_arm);
    licensePool()->addLicense(QnLicensePtr(stub));
}

void QnLicensePoolScaffold::addLicense(Qn::LicenseType licenseType) {
    addLicenses(licenseType, 1);
}

void QnLicensePoolScaffold::addFutureLicenses(int count) {
    auto stub = new QnFutureLicenseStub(count);
    licensePool()->addLicense(QnLicensePtr(stub));
}
