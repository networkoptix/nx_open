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

    int totalDigital() const;
    int totalAnalog() const;

    int usedDigital() const;
    int usedAnalog() const;

    int requiredDigital() const;
    int requiredAnalog() const;

    bool isValid() const;
private:
    void update();

    QnLicenseList m_licenses;

    int m_usedDigital;
    int m_usedAnalog;

    int m_requiredDigital;
    int m_requiredAnalog;

    int m_proposedDigital;
    int m_proposedAnalog;

    bool m_isValid;
};

#endif // LICENSE_USAGE_HELPER_H
