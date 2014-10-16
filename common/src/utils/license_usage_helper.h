#ifndef LICENSE_USAGE_HELPER_H
#define LICENSE_USAGE_HELPER_H

#include <core/resource/resource_fwd.h>
#include <licensing/license.h>

#include <utils/common/connective.h>

static const QString QN_LICENSE_URL(lit("http://networkoptix.com/nolicensed_vms/activate.php"));

class QnLicenseUsageHelper: public Connective<QObject>
{
    Q_OBJECT
    typedef  Connective<QObject> base_type;
public:
    QnLicenseUsageHelper(QObject *parent = NULL);

    bool isValid() const;

    bool isValid(Qn::LicenseType licenseType) const;

    /** 
     *  Get text "Activate %n more licenses" or "%n more licenses will be used" if valid for the selected type.
     */

    QString getRequiredLicenseMsg(Qn::LicenseType licenseType) const;

    /** 
     *  Get text "Activate %n more licenses" or "%n more licenses will be used" if valid for all types.
     */
    QString getRequiredLicenseMsg() const;

    /**
     *  Get text "%n licenses are used out of %n." for the selected type.
     */
    QString getUsageText(Qn::LicenseType licenseType) const;

    /**
     *  Get text "%n licenses are used out of %n." for all types.
     */
    QString getUsageText() const;

    /**
     *  Get text "%n licenses will be used out of %n." for the selected type.
     */
    QString getProposedUsageText(Qn::LicenseType licenseType) const;

    /**
     *  Get text "%n licenses will be used out of %n." for all types.
     */
    QString getProposedUsageText() const;

    int totalLicense(Qn::LicenseType licenseType) const;
    int usedLicense(Qn::LicenseType licenseType) const;

    virtual QList<Qn::LicenseType> licenseTypes() const;

    /**
     *  Get full error message from activation server
     */
    static QString activationMessage(const QJsonObject& errorMessage);

    void update();
signals:
     void licensesChanged();

protected:
    void borrowLicenseFromClass(int& srcUsed, int srcTotal, int& dstUsed, int dstTotal);
    virtual int calculateUsedLicenses(Qn::LicenseType licenseType) const = 0;
    virtual QList<Qn::LicenseType> calculateLicenseTypes() const = 0;

    QnLicenseListHelper m_licenses;
    mutable QList<Qn::LicenseType> m_licenseTypes;

    int m_usedLicenses[Qn::LC_Count];
    int m_proposedLicenses[Qn::LC_Count];
    int m_overflowLicenses[Qn::LC_Count];
};

class QnCamLicenseUsageHelper: public QnLicenseUsageHelper {
    Q_OBJECT
public:
    QnCamLicenseUsageHelper(QObject *parent = NULL);
    QnCamLicenseUsageHelper(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable, QObject *parent = NULL);

    void propose(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable);
    bool isOverflowForCamera(const QnVirtualCameraResourcePtr &camera);
protected:
    virtual QList<Qn::LicenseType> calculateLicenseTypes() const override;
    virtual int calculateUsedLicenses(Qn::LicenseType licenseType) const override;
private:
    void init();
};

class QnVideoWallLicenseUsageHelper: public QnLicenseUsageHelper {
    Q_OBJECT
public:
    QnVideoWallLicenseUsageHelper(QObject *parent = NULL);

    /** Propose to use itemsCount videowall items on the pc given for the given videowall. */
    void propose(const QnVideoWallResourcePtr &videowall, const QnUuid &pcUuid, int itemsCount);

    /** Propose to use some more or less licenses directly (e.g. to start control session). */
    void propose(int count);

protected:
    virtual QList<Qn::LicenseType> calculateLicenseTypes() const override;
    virtual int calculateUsedLicenses(Qn::LicenseType licenseType) const override;
};

/** Utility RAAA class to propose some licenses usage. */
class QnVideoWallLicenseUsageProposer {
public:
    QnVideoWallLicenseUsageProposer(QnVideoWallLicenseUsageHelper* helper, int count);
    ~QnVideoWallLicenseUsageProposer();
private:
    QPointer<QnVideoWallLicenseUsageHelper> m_helper;
    int m_count;
};

#endif // LICENSE_USAGE_HELPER_H
