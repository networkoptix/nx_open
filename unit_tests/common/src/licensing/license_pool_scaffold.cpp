#include "license_pool_scaffold.h"

#include <licensing/license_stub.h>

QnLicensePoolScaffold::QnLicensePoolScaffold() {
    qnLicensePool;
}

QnLicensePoolScaffold::~QnLicensePoolScaffold() {
    qnLicensePool->reset();
}

void QnLicensePoolScaffold::addLicenses(Qn::LicenseType licenseType, int count) {
    qnLicensePool->addLicense(QnLicensePtr(new QnLicenseStub(licenseType, count)));
}

void QnLicensePoolScaffold::addLicense(Qn::LicenseType licenseType) {
    addLicenses(licenseType, 1);
}
