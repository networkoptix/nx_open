#ifndef LICENSE_USAGE_HELPER_H
#define LICENSE_USAGE_HELPER_H

#include <core/resource/resource_fwd.h>
#include <licensing/license.h>

class QnLicenseUsageHelper: public QObject
{
    Q_OBJECT
public:

    QnLicenseUsageHelper();
    QnLicenseUsageHelper(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable);

    void propose(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable);

    bool isValid() const;

    QString getRequiredLicenseMsg() const;
    QString getUsageText(Qn::LicenseType licenseType) const;
    QString getUsageText() const;
    QString getWillUsageText(Qn::LicenseType licenseType) const;
    QString getWillUsageText() const;
    bool isOverflowForCamera(QnVirtualCameraResourcePtr camera);

    int totalLicense(Qn::LicenseType licenseType) const;
    int usedLicense(Qn::LicenseType licenseType) const;

    void update();
signals:
    void updated();
private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &);
    void at_resourcePool_statusChanged(const QnResourcePtr &);
private:
    void init();
    void borowLicenseFromClass(int& srcUsed, int srcTotal, int& dstUsed, int dstTotal);

    QnLicenseListHelper m_licenses;

    int m_usedLicenses[Qn::LC_CountTotal];
    int m_proposedLicenses[Qn::LC_CountTotal];
    int m_overflowLicenses[Qn::LC_CountTotal];

    bool m_isValid;
};

#endif // LICENSE_USAGE_HELPER_H
