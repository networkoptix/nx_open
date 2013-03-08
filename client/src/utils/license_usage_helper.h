#ifndef LICENSE_USAGE_HELPER_H
#define LICENSE_USAGE_HELPER_H

#include <licensing/license.h>

class QnLicenseUsageHelper
{
public:
    QnLicenseUsageHelper();

    int totalDigital();
    int totalAnalog();

    int usedDigital();
    int usedAnalog();

    int requiredDigital();
    int requiredAnalog();

    bool isValid();
private:
    QnLicenseList m_licenses;
};

#endif // LICENSE_USAGE_HELPER_H
