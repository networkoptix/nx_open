#ifndef QN_LICENSE_POOL_SCAFFOLD
#define QN_LICENSE_POOL_SCAFFOLD

#include <licensing/license.h>

/** RAII class for managing license pool. */
class QnLicensePoolScaffold {
public:
    QnLicensePoolScaffold();
    ~QnLicensePoolScaffold();

    void addLicense(Qn::LicenseType licenseType, int count);
};

#endif QN_LICENSE_POOL_SCAFFOLD
