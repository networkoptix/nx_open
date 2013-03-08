#ifndef LICENSE_USAGE_HELPER_H
#define LICENSE_USAGE_HELPER_H

#include <core/resource/resource_fwd.h>
#include <licensing/license.h>

class QnLicenseUsageHelper
{
public:
    QnLicenseUsageHelper();
    QnLicenseUsageHelper(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable);

    int totalDigital();
    int totalAnalog();

    int usedDigital();
    int usedAnalog();

    int requiredDigital();
    int requiredAnalog();

    bool isValid();
private:
    QnLicenseList m_licenses;

    int m_digitalChange;
    int m_analogChange;
};

#endif // LICENSE_USAGE_HELPER_H
