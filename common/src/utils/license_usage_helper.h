#ifndef LICENSE_USAGE_HELPER_H
#define LICENSE_USAGE_HELPER_H

#include <core/resource/resource_fwd.h>
#include <licensing/license.h>

class QnLicenseUsageHelper
{
public:

    QnLicenseUsageHelper();
    QnLicenseUsageHelper(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable);

    void propose(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable);

    bool isValid() const;

    QString getRequiredLicenseMsg() const;
    QString getUsageText(Qn::LicenseClass licenseClass) const;
    QString getWillUsageText(Qn::LicenseClass licenseClass) const;
    bool isOverflowForCamera(QnVirtualCameraResourcePtr camera);

    int totalLicense(Qn::LicenseClass licenseClass) const;
    int usedLicense(Qn::LicenseClass licenseClass) const;

    QString longClassName(Qn::LicenseClass licenseClass) const;

    void update();
private:
    void init();
    void borowLicenseFromClass(int& srcUsed, int srcTotal, int& dstUsed, int dstTotal);

    QnLicenseListHelper m_licenses;

    int m_usedLicenses[Qn::LC_Count];
    int m_proposedLicenses[Qn::LC_Count];
    int m_overflowLicenses[Qn::LC_Count];

    bool m_isValid;
};

#endif // LICENSE_USAGE_HELPER_H
