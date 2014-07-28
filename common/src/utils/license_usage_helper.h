#ifndef LICENSE_USAGE_HELPER_H
#define LICENSE_USAGE_HELPER_H

#include <core/resource/resource_fwd.h>
#include <licensing/license.h>

class QnLicenseUsageHelper: public QObject
{
    Q_OBJECT
public:
    bool isValid() const;

    QString getRequiredLicenseMsg() const;
    QString getUsageText(Qn::LicenseType licenseType) const;
    QString getUsageText() const;
    QString getProposedUsageText(Qn::LicenseType licenseType) const;
    QString getProposedUsageText() const;

    int totalLicense(Qn::LicenseType licenseType) const;
    int usedLicense(Qn::LicenseType licenseType) const;

    virtual QList<Qn::LicenseType> licenseTypes() const = 0;
protected:
    /* This class should not be used directly. */
    QnLicenseUsageHelper();

    void borrowLicenseFromClass(int& srcUsed, int srcTotal, int& dstUsed, int dstTotal);

    QnLicenseListHelper m_licenses;

    int m_usedLicenses[Qn::LC_Count];
    int m_proposedLicenses[Qn::LC_Count];
    int m_overflowLicenses[Qn::LC_Count];

    bool m_isValid;
};

class QnCamLicenseUsageHelper: public QnLicenseUsageHelper {
    Q_OBJECT
public:
    QnCamLicenseUsageHelper();
    QnCamLicenseUsageHelper(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable);

    void propose(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable);
    bool isOverflowForCamera(const QnVirtualCameraResourcePtr &camera);

    void update();

    virtual QList<Qn::LicenseType> licenseTypes() const override;
private:
    void init();
};

class QnVideoWallLicenseUsageHelper: public QnLicenseUsageHelper {
    Q_OBJECT
public:
    QnVideoWallLicenseUsageHelper();

    void update();

    virtual QList<Qn::LicenseType> licenseTypes() const override;
};

#endif // LICENSE_USAGE_HELPER_H
