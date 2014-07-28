#ifndef LICENSE_USAGE_HELPER_H
#define LICENSE_USAGE_HELPER_H

#include <core/resource/resource_fwd.h>
#include <licensing/license.h>

#include <utils/common/connective.h>

class QnLicenseUsageHelper: public Connective<QObject>
{
    Q_OBJECT
    typedef  Connective<QObject> base_type;
public:
    QnLicenseUsageHelper(QObject *parent = NULL);

    bool isValid() const;

    QString getRequiredLicenseMsg() const;
    QString getUsageText(Qn::LicenseType licenseType) const;
    QString getUsageText() const;
    QString getProposedUsageText(Qn::LicenseType licenseType) const;
    QString getProposedUsageText() const;

    int totalLicense(Qn::LicenseType licenseType) const;
    int usedLicense(Qn::LicenseType licenseType) const;

    virtual QList<Qn::LicenseType> licenseTypes() const = 0;

     void update();
signals:
     void licensesChanged();

protected:
    void borrowLicenseFromClass(int& srcUsed, int srcTotal, int& dstUsed, int dstTotal);
    virtual int calculateUsedLicenses(Qn::LicenseType licenseType) const = 0;

    QnLicenseListHelper m_licenses;

    int m_usedLicenses[Qn::LC_Count];
    int m_proposedLicenses[Qn::LC_Count];
    int m_overflowLicenses[Qn::LC_Count];

    bool m_isValid;
};

class QnCamLicenseUsageHelper: public QnLicenseUsageHelper {
    Q_OBJECT
public:
    QnCamLicenseUsageHelper(QObject *parent = NULL);
    QnCamLicenseUsageHelper(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable, QObject *parent = NULL);

    void propose(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable);
    bool isOverflowForCamera(const QnVirtualCameraResourcePtr &camera);

    virtual QList<Qn::LicenseType> licenseTypes() const override;
protected:
    virtual int calculateUsedLicenses(Qn::LicenseType licenseType) const override;
private:
    void init();
};

class QnVideoWallLicenseUsageHelper: public QnLicenseUsageHelper {
    Q_OBJECT
public:
    QnVideoWallLicenseUsageHelper(QObject *parent = NULL);

    virtual QList<Qn::LicenseType> licenseTypes() const override;
protected:
    virtual int calculateUsedLicenses(Qn::LicenseType licenseType) const override;
};

#endif // LICENSE_USAGE_HELPER_H
