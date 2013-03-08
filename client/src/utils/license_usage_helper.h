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

    int proposedDigital() const;
    int proposedAnalog() const;

    bool isValid() const;
private:
    QnLicenseList m_licenses;

    int m_digitalChange;
    int m_analogChange;
};

#endif // LICENSE_USAGE_HELPER_H
