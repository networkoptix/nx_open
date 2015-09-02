#ifndef QN_LICENSE_POOL_SCAFFOLD
#define QN_LICENSE_POOL_SCAFFOLD

#include <licensing/license.h>

/** RAII class for managing license pool. */
class QnLicensePoolScaffold {
public:
    QnLicensePoolScaffold(bool isArm = false);
    ~QnLicensePoolScaffold();

    void addLicense(Qn::LicenseType licenseType);
    void addLicenses(Qn::LicenseType licenseType, int count);

    void addFutureLicenses(int count);
private:
    /** Are we emulating the arm server. */
    bool m_arm;
};

#endif  // QN_LICENSE_POOL_SCAFFOLD
