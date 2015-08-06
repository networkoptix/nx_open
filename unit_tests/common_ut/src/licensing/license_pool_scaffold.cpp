#include "license_pool_scaffold.h"

#include <licensing/license_stub.h>

QnLicensePoolScaffold::QnLicensePoolScaffold(bool isArm /* = false */):
    m_arm(isArm)
{
    qnLicensePool;
}

QnLicensePoolScaffold::~QnLicensePoolScaffold() {
    qnLicensePool->reset();
}

void QnLicensePoolScaffold::addLicenses(Qn::LicenseType licenseType, int count) {
    auto stub = new QnLicenseStub(licenseType, count);
    stub->setArmServer(m_arm);
    qnLicensePool->addLicense(QnLicensePtr(stub));
}

void QnLicensePoolScaffold::addLicense(Qn::LicenseType licenseType) {
    addLicenses(licenseType, 1);
}
