// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>

#include <QtCore/QPointer>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>
#include <licensing/license.h>
#include <nx/utils/url.h>
#include <utils/common/connective.h>
#include "list_helper.h"

namespace nx::vms::license {

struct LicenseServer
{
    static const nx::utils::Url indexUrl(QnCommonModule* commonModule);
    static const nx::utils::Url activateUrl(QnCommonModule* commonModule);
    static const nx::utils::Url deactivateUrl(QnCommonModule* commonModule);
    static const nx::utils::Url validateUrl(QnCommonModule* commonModule);
private:
    static const QString baseUrl(QnCommonModule* commonModule);
};

struct LicenseCompatibility;

enum class UsageStatus
{
    invalid,
    notUsed,
    overflow,
    used
};

class UsageWatcher:
    public Connective<QObject>,
    public /*mixin*/ QnCommonModuleAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;
public:
    UsageWatcher(QnCommonModule* commonModule, QObject* parent = nullptr);

signals:
    void licenseUsageChanged();
};

typedef std::array<int, Qn::LC_Count> licensesArray;

class UsageHelper:
    public Connective<QObject>,
    public /*mixin*/ QnCommonModuleAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;
public:
    UsageHelper(QnCommonModule* commonModule, QObject* parent = nullptr);
    virtual ~UsageHelper() override;

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

    /** Custom validator (e.g. for unit tests). */
    void setCustomValidator(std::unique_ptr<Validator> validator);

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

        ListHelper licenses;
        licensesArray total;
        licensesArray used;
        licensesArray proposed;
        licensesArray overflow;
    };

    mutable Cache m_cache;

    std::unique_ptr<Validator> m_validator;
    QTimer m_invalidateTimer;
};

class CamLicenseUsageWatcher: public UsageWatcher
{
    Q_OBJECT
    using base_type = UsageWatcher;
public:
    CamLicenseUsageWatcher(QnCommonModule* commonModule, QObject* parent = nullptr);
    CamLicenseUsageWatcher(const QnVirtualCameraResourcePtr& camera,
        QnCommonModule* commonModule,
        QObject* parent = nullptr);
};

class CamLicenseUsageHelper: public UsageHelper
{
    Q_OBJECT
    using base_type = UsageHelper;
public:
    /*
        Constructors. Each one uses specified watcher or create a new one if parameter is empty.
        With empty watcher parameter creates instance which tracks all cameras.
    */
    CamLicenseUsageHelper(QnCommonModule* commonModule, QObject* parent = nullptr);

    CamLicenseUsageHelper(
        const QnVirtualCameraResourceList &proposedCameras,
        bool proposedEnable,
        QnCommonModule* commonModule,
        QObject* parent = nullptr);

    CamLicenseUsageHelper(
        const QnVirtualCameraResourcePtr &proposedCamera,
        bool proposedEnable,
        QnCommonModule* commonModule,
        QObject* parent = nullptr);

    void propose(const QnVirtualCameraResourcePtr &proposedCamera, bool proposedEnable);
    void propose(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable);
    bool isOverflowForCamera(const QnVirtualCameraResourcePtr &camera);
    bool isOverflowForCamera(const QnVirtualCameraResourcePtr &camera, bool cachedLicenseUsed);

    bool canEnableRecording(const QnVirtualCameraResourcePtr& proposedCamera);
    bool canEnableRecording(const QnVirtualCameraResourceList& proposedCameras);

signals:
    void licenseUsageChanged();

protected:
    virtual QList<Qn::LicenseType> calculateLicenseTypes() const override;
    virtual void calculateUsedLicenses(licensesArray& basicUsedLicenses, licensesArray& proposedToUse) const override;

private:
    CamLicenseUsageWatcher* m_watcher{nullptr};
    QSet<QnVirtualCameraResourcePtr> m_proposedToEnable;
    QSet<QnVirtualCameraResourcePtr> m_proposedToDisable;
};

class SingleCamLicenseStatusHelper: public Connective<QObject>
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    explicit SingleCamLicenseStatusHelper(const QnVirtualCameraResourcePtr &camera,
        QObject* parent = nullptr);
    virtual ~SingleCamLicenseStatusHelper();

    UsageStatus status() const;

signals:
    void licenseStatusChanged();

private:
    const QnVirtualCameraResourcePtr m_camera;
    QScopedPointer<CamLicenseUsageHelper> m_helper;
};

class VideoWallLicenseUsageWatcher: public UsageWatcher
{
    Q_OBJECT
    using base_type = UsageWatcher;
public:
    VideoWallLicenseUsageWatcher(QnCommonModule* commonModule, QObject* parent = nullptr);
};

class VideoWallLicenseUsageHelper: public UsageHelper
{
    Q_OBJECT
    using base_type = UsageHelper;
public:
    VideoWallLicenseUsageHelper(QnCommonModule* commonModule, QObject* parent = nullptr);

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
class VideoWallLicenseUsageProposer
{
public:
    VideoWallLicenseUsageProposer(
        VideoWallLicenseUsageHelper* helper,
        int screenCount);
    ~VideoWallLicenseUsageProposer();
private:
    QPointer<VideoWallLicenseUsageHelper> m_helper;
    int m_count;
};

} // namespace nx::vms::license
