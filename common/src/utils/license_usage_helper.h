#ifndef LICENSE_USAGE_HELPER_H
#define LICENSE_USAGE_HELPER_H

#include <array>
#include <core/resource/resource_fwd.h>
#include <licensing/license.h>

#include <utils/common/connective.h>

static const QString QN_LICENSE_URL(lit("http://licensing.networkoptix.com/nxlicensed/activate.php"));

struct LicenseCompatibility;

class QnLicenseUsageWatcher: public Connective<QObject> {
    Q_OBJECT

   typedef Connective<QObject> base_type;
public:
    QnLicenseUsageWatcher(QObject* parent = NULL);
signals:
    void licenseUsageChanged();
};

typedef std::array<int, Qn::LC_Count> licensesArray;

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

    struct Cache {
        Cache();

        QnLicenseListHelper licenses;
        licensesArray total;
        licensesArray used;
        licensesArray proposed;
        licensesArray overflow;
    };

    mutable Cache m_cache;
};

class QnCamLicenseUsageWatcher: public QnLicenseUsageWatcher {
    Q_OBJECT

    typedef QnLicenseUsageWatcher base_type;
public:
    QnCamLicenseUsageWatcher(QObject *parent = nullptr);
    QnCamLicenseUsageWatcher(const QnVirtualCameraResourcePtr &camera, QObject *parent = nullptr);

private:
    void init(const QnVirtualCameraResourcePtr &camera);
};

typedef QSharedPointer<QnCamLicenseUsageWatcher> QnCamLicenseUsageWatcherPtr;

class QnCamLicenseUsageHelper: public QnLicenseUsageHelper {
    Q_OBJECT

    typedef QnLicenseUsageHelper base_type;
public:
    /*
        Constructors. Each one uses specified watcher or create a new one if parameter is empty.
        With empty watcher parameter creates instance which tracks all cameras.
    */

    QnCamLicenseUsageHelper(const QnCamLicenseUsageWatcherPtr &watcher = QnCamLicenseUsageWatcherPtr()
        , QObject *parent = NULL);

    QnCamLicenseUsageHelper(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable
        , const QnCamLicenseUsageWatcherPtr &watcher = QnCamLicenseUsageWatcherPtr(), QObject *parent = NULL);

    QnCamLicenseUsageHelper(const QnVirtualCameraResourcePtr &proposedCamera, bool proposedEnable
        , const QnCamLicenseUsageWatcherPtr &watcher = QnCamLicenseUsageWatcherPtr(), QObject *parent = NULL);

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
    void init(const QnCamLicenseUsageWatcherPtr &watcher);

    QnCamLicenseUsageWatcherPtr m_watcher;
    QSet<QnVirtualCameraResourcePtr> m_proposedToEnable;
    QSet<QnVirtualCameraResourcePtr> m_proposedToDisable;
};

class QnSingleCamLicenceStatusHelper : public Connective<QObject>
{
    Q_OBJECT

public:
    enum CameraLicenseStatus
    {
        InvalidSource
        , LicenseNotUsed
        , LicenseOverflow
        , LicenseUsed
    };

    QnSingleCamLicenceStatusHelper(const QnVirtualCameraResourcePtr &camera);

    virtual ~QnSingleCamLicenceStatusHelper();

    CameraLicenseStatus status();

signals:
    void licenceStatusChanged();

private:
    typedef QScopedPointer<QnCamLicenseUsageHelper> QnCamLicenseUsageHelperPtr;

    const QnVirtualCameraResourcePtr m_camera;
    const QnCamLicenseUsageHelperPtr m_helper;
};

class QnVideoWallLicenseUsageWatcher: public QnLicenseUsageWatcher {
    Q_OBJECT

    typedef QnLicenseUsageWatcher base_type;
public:
    QnVideoWallLicenseUsageWatcher(QObject* parent = NULL);
};

class QnVideoWallLicenseUsageHelper: public QnLicenseUsageHelper {
    Q_OBJECT
public:
    QnVideoWallLicenseUsageHelper(QObject *parent = NULL);

    /** Propose to use some more or less licenses directly (e.g. to start control session). */
    void propose(int count);

    /** Calculate how many licenses are required for the given screens count. */
    static int licensesForScreens(int screens);
protected:
    virtual QList<Qn::LicenseType> calculateLicenseTypes() const override;
    virtual void calculateUsedLicenses(licensesArray& basicUsedLicenses, licensesArray& proposedToUse) const override;
private:
    int m_proposed;
};

/** Utility RAAA class to propose some licenses usage. */
class QnVideoWallLicenseUsageProposer {
public:
    QnVideoWallLicenseUsageProposer(QnVideoWallLicenseUsageHelper* helper, int screenCount, int controlSessionsCount);
    ~QnVideoWallLicenseUsageProposer();
private:
    QPointer<QnVideoWallLicenseUsageHelper> m_helper;
    int m_count;
};

#endif // LICENSE_USAGE_HELPER_H
