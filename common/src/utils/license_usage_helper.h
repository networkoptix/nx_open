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
	int totalEdge() const;

    int usedDigital() const;
    int usedAnalog() const;
    int usedEdge() const;

    int overflowDigital() const;
    int overflowAnalog() const;
    int overflowEdge() const;

    bool isValid() const;

    QString getRequiredLicenseMsg() const;
    QString getUsageText(Qn::LicenseClass licenseClass) const;
    bool isOverflowForCamera(QnVirtualCameraResourcePtr camera);

    void update();
private:
    void init();
    void borowLicenseFromClass(int& srcUsed, int srcTotal, int& dstUsed, int dstTotal);

    QnLicenseListHelper m_licenses;

    int m_usedDigital;
    int m_usedAnalog;
    int m_usedEdge;

    //int m_required;

    int m_proposedDigital;
    int m_proposedAnalog;
    int m_proposedEdge;

    bool m_isValid;

    int m_edgeOverflow;
    int m_digitalOverflow;
    int m_analogOverflow;
};

#endif // LICENSE_USAGE_HELPER_H
