#pragma once

#include <array>

#include <QtCore/QPointer>

#include <core/resource/resource_fwd.h>
#include <licensing/license.h>

#include <utils/common/connective.h>
#include <common/common_module_aware.h>

static const QString QN_LICENSE_URL(lit("http://licensing.networkoptix.com/nxlicensed/activate.php"));

struct LicenseCompatibility;

class QnLicenseUsageWatcher: public Connective<QObject>, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;
public:
    QnLicenseUsageWatcher(QnCommonModule* commonModule, QObject* parent = nullptr);

signals:
    void licenseUsageChanged();
};

typedef std::array<int, Qn::LC_Count> licensesArray;

class QnLicenseUsageHelper: public Connective<QObject>, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;
public:
    QnLicenseUsageHelper(QnCommonModule* commonModule, QObject* parent = nullptr);

    bool isValid() const;

    bool isValid(Qn::LicenseType licenseType) const;

    /**
     *  Get text "Activate %n more licenses" or "%n more licenses will be used" if valid for the selected type.
     */

    QString getRequiredText(Qn::LicenseType licenseType) const;

    /**
     *  Get text "Activate %n more licenses" or "%n more licenses will be used" if valid for all types.
     */
    QString getRequiredMsg() const;

    /**
     *  Get text "%n licenses are used out of %n." for the selected type.
     */
    QString getUsageText(Qn::LicenseType licenseType) const;

    /**
     *  Get text "%n licenses are used out of %n." for all types.
     */
    QString getUsageMsg() const;

    /**
     *  Get text "%n licenses will be used out of %n." for the selected type.
     */
    QString getProposedUsageText(Qn::LicenseType licenseType) const;

    /**
     *  Get text "%n licenses will be used out of %n." for all types.
     */
    QString getProposedUsageMsg() const;

    /** Number of valid licenses of the selected type. */
    int totalLicenses(Qn::LicenseType licenseType) const;

    /** Number of licenses of the selected type currently in use (including proposed). */
    int usedLicenses(Qn::LicenseType licenseType) const;

    /**
     *  Number of licenses of the selected type lacking for system to work.
     *  Always equals to 0 of the helper is valid.
     */
    int requiredLicenses(Qn::LicenseType licenseType) const;

    /**
     *  Number of licenses that are proposed to be used.
     *  Makes no sense if the helper is NOT valid.
     */
    int proposedLicenses(Qn::LicenseType licenseType) const;

    virtual QList<Qn::LicenseType> licenseTypes() const;

    /** Mark data as invalid and needs to be recalculated. */
    void invalidate();

    /** Custom validator (e.g. for unit tests). Does not take ownership.  */
    void setCustomValidator(QnLicenseValidator* validator);

protected:
    virtual void calculateUsedLicenses(licensesArray& basicUsedLicenses, licensesArray& proposedToUse) const = 0;
    virtual int calculateOverflowLicenses(Qn::LicenseType licenseType, int borrowedLicenses) const;

    virtual QList<Qn::LicenseType> calculateLicenseTypes() const = 0;

    void updateCache() const;
private:
    int borrowLicenses(const LicenseCompatibility &compat, licensesArray &licenses) const;

private:
    mutable bool m_dirty;
    mutable QList<Qn::LicenseType> m_licenseTypes;

    struct Cache
    {
        Cache();

        QnLicenseListHelper licenses;
        licensesArray total;
        licensesArray used;
        licensesArray proposed;
        licensesArray overflow;
    };

    mutable Cache m_cache;

    QPointer<QnLicenseValidator> m_validator;
};

class QnCamLicenseUsageWatcher: public QnLicenseUsageWatcher
{
    Q_OBJECT
    using base_type = QnLicenseUsageWatcher;
public:
    QnCamLicenseUsageWatcher(QnCommonModule* commonModule, QObject* parent = nullptr);
    QnCamLicenseUsageWatcher(const QnVirtualCameraResourcePtr& camera,
        QnCommonModule* commonModule,
        QObject* parent = nullptr);
};

class QnCamLicenseUsageHelper: public QnLicenseUsageHelper
{
    Q_OBJECT
    using base_type = QnLicenseUsageHelper;
public:
    /*
        Constructors. Each one uses specified watcher or create a new one if parameter is empty.
        With empty watcher parameter creates instance which tracks all cameras.
    */
    QnCamLicenseUsageHelper(QnCommonModule* commonModule, QObject* parent = nullptr);

    QnCamLicenseUsageHelper(
        const QnVirtualCameraResourceList &proposedCameras,
        bool proposedEnable,
        QnCommonModule* commonModule,
        QObject* parent = nullptr);

    QnCamLicenseUsageHelper(
        const QnVirtualCameraResourcePtr &proposedCamera,
        bool proposedEnable,
        QnCommonModule* commonModule,
        QObject* parent = nullptr);

public:
    void propose(const QnVirtualCameraResourcePtr &proposedCamera, bool proposedEnable);
    void propose(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable);
    bool isOverflowForCamera(const QnVirtualCameraResourcePtr &camera);
    bool isOverflowForCamera(const QnVirtualCameraResourcePtr &camera, bool cachedLicenceUsed);

signals:
    void licenseUsageChanged();

protected:
    virtual QList<Qn::LicenseType> calculateLicenseTypes() const override;
    virtual void calculateUsedLicenses(licensesArray& basicUsedLicenses, licensesArray& proposedToUse) const override;

private:
    QnCamLicenseUsageWatcher* m_watcher{nullptr};
    QSet<QnVirtualCameraResourcePtr> m_proposedToEnable;
    QSet<QnVirtualCameraResourcePtr> m_proposedToDisable;
};

class QnSingleCamLicenseStatusHelper: public Connective<QObject>
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    enum class LicenseStatus
    {
        invalid,
        notUsed,
        overflow,
        used
    };

    explicit QnSingleCamLicenseStatusHelper(const QnVirtualCameraResourcePtr &camera,
        QObject* parent = nullptr);
    virtual ~QnSingleCamLicenseStatusHelper();

    LicenseStatus status() const;

signals:
    void licenseStatusChanged();

private:
    const QnVirtualCameraResourcePtr m_camera;
    QScopedPointer<QnCamLicenseUsageHelper> m_helper;
};

class QnVideoWallLicenseUsageWatcher: public QnLicenseUsageWatcher
{
    Q_OBJECT
    using base_type = QnLicenseUsageWatcher;
public:
    QnVideoWallLicenseUsageWatcher(QnCommonModule* commonModule, QObject* parent = nullptr);
};

class QnVideoWallLicenseUsageHelper: public QnLicenseUsageHelper
{
    Q_OBJECT
    using base_type = QnLicenseUsageHelper;
public:
    QnVideoWallLicenseUsageHelper(QnCommonModule* commonModule, QObject* parent = nullptr);

    /** Propose to use some more or less licenses directly (e.g. to start control session). */
    void propose(int count);

    /** Calculate how many licenses are required for the given screens count. */
    static int licensesForScreens(int screens);

protected:
    virtual QList<Qn::LicenseType> calculateLicenseTypes() const override;
    virtual void calculateUsedLicenses(licensesArray& basicUsedLicenses, licensesArray& proposedToUse) const override;

private:
    int m_proposed = 0;
};

/** Utility RAAA class to propose some licenses usage. */
class QnVideoWallLicenseUsageProposer
{
public:
    QnVideoWallLicenseUsageProposer(
        QnVideoWallLicenseUsageHelper* helper,
        int screenCount,
        int controlSessionsCount);
    ~QnVideoWallLicenseUsageProposer();
private:
    QPointer<QnVideoWallLicenseUsageHelper> m_helper;
    int m_count;
};
