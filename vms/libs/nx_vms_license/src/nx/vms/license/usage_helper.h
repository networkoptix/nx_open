// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>

#include <QtCore/QPointer>

#include <core/resource/resource_fwd.h>
#include <licensing/license.h>
#include <nx/network/aio/repetitive_timer.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/url.h>
#include <nx/vms/common/system_context_aware.h>

#include "list_helper.h"

namespace nx::vms::license {

struct LicenseServer
{
    static const nx::utils::Url indexUrl(common::SystemContext* context);
    static const nx::utils::Url activateUrl(common::SystemContext* context);
    static const nx::utils::Url activateV2Url(common::SystemContext* context);
    static const nx::utils::Url deactivateUrl(common::SystemContext* context);
    static const nx::utils::Url validateUrl(common::SystemContext* context);
    static const nx::utils::Url cloudLicensesUrl(common::SystemContext* context);
    static const nx::utils::Url inspectUrl(common::SystemContext* context);
private:
    static const QString baseUrl(common::SystemContext* context);
};

struct ChannelPartnerServer
{
    static const nx::utils::Url reportUrl(const common::SystemSettings* settings);
    static const nx::utils::Url saasServicesUrl(const common::SystemSettings* settings);
    static const nx::utils::Url saasDataUrl(const common::SystemSettings* settings);

    static const QString baseUrl(const common::SystemSettings* settings);
};

struct LicenseCompatibility;

enum class UsageStatus
{
    invalid,
    notUsed,
    overflow,
    used
};

typedef std::array<int, Qn::LC_Count> LicensesArray;

using LicenseUsageByDevice = QVector<QnUuid>;
using LicensesArrayEx =  std::array<LicenseUsageByDevice, Qn::LC_Count>;

class UsageHelper:
    public QObject,
    public /*mixin*/ common::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;
public:
    UsageHelper(common::SystemContext* context, QObject* parent = nullptr);
    virtual ~UsageHelper() override;

    virtual bool isValid() const;

    virtual bool isValid(Qn::LicenseType licenseType) const;

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

    /** @return the list of Devices that spend requested license type. */
    std::set<QnUuid> usedDevices(Qn::LicenseType licenseType) const;

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
    virtual void calculateUsedLicenses(LicensesArrayEx& basicUsedLicenses, LicensesArrayEx& proposedToUse) const = 0;

    virtual QList<Qn::LicenseType> calculateLicenseTypes() const = 0;

    void updateCache() const;
private:
    LicenseUsageByDevice borrowLicenses(const LicenseCompatibility& compat, LicensesArrayEx& licenses) const;

private:
    mutable bool m_dirty;
    mutable QList<Qn::LicenseType> m_licenseTypes;

    struct Cache
    {
        Cache();

        void reset();

        ListHelper licenses;
        LicensesArray total;
        LicensesArrayEx used;
        LicensesArray proposed;
        LicensesArrayEx overflow;
    };

    mutable Cache m_cache;

    std::unique_ptr<Validator> m_validator;
    mutable nx::utils::ElapsedTimer m_invalidateTimer;
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
    CamLicenseUsageHelper(
        common::SystemContext* context,
        bool considerOnlineServersOnly = false,
        QObject* parent = nullptr);

    CamLicenseUsageHelper(
        const QnVirtualCameraResourceList& proposedCameras,
        bool proposedEnable,
        common::SystemContext* context,
        QObject* parent = nullptr);

    CamLicenseUsageHelper(
        const QnVirtualCameraResourcePtr& proposedCamera,
        bool proposedEnable,
        common::SystemContext* context,
        QObject* parent = nullptr);

    void propose(const QnVirtualCameraResourcePtr &proposedCamera, bool proposedEnable);
    void propose(const QnVirtualCameraResourceList &proposedCameras, bool proposedEnable);
    bool isOverflowForCamera(const QnVirtualCameraResourcePtr &camera) const;
    bool isOverflowForCamera(const QnVirtualCameraResourcePtr &camera, bool cachedLicenseUsed) const;

    bool canEnableRecording(const QnVirtualCameraResourcePtr& proposedCamera);
    bool canEnableRecording(const QnVirtualCameraResourceList& proposedCameras);

signals:
    void licenseUsageChanged();

protected:
    virtual QList<Qn::LicenseType> calculateLicenseTypes() const override;
    virtual void calculateUsedLicenses(LicensesArrayEx& basicUsedLicenses, LicensesArrayEx& proposedToUse) const override;

private:
    QSet<QnVirtualCameraResourcePtr> m_proposedToEnable;
    QSet<QnVirtualCameraResourcePtr> m_proposedToDisable;
    bool m_considerOnlineServersOnly = false;
};

class SingleCamLicenseStatusHelper: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit SingleCamLicenseStatusHelper(
        const QnVirtualCameraResourcePtr &camera,
        QObject* parent = nullptr);
    virtual ~SingleCamLicenseStatusHelper();

    UsageStatus status() const;

signals:
    void licenseStatusChanged();

private:
    const QnVirtualCameraResourcePtr m_camera;
    QScopedPointer<CamLicenseUsageHelper> m_helper;
};

class VideoWallLicenseUsageHelper: public UsageHelper
{
    Q_OBJECT
    using base_type = UsageHelper;
public:
    VideoWallLicenseUsageHelper(common::SystemContext* context, QObject* parent = nullptr);

    virtual bool isValid() const override;
    virtual bool isValid(Qn::LicenseType licenseType) const override;

    /** Propose to use some more or less licenses directly (e.g. to start control session). */
    void propose(int count);

    /** Calculate how many licenses are required for the given screens count. */
    static int licensesForScreens(int screens);

protected:
    virtual QList<Qn::LicenseType> calculateLicenseTypes() const override;
    virtual void calculateUsedLicenses(
        LicensesArrayEx& basicUsedLicenses,
        LicensesArrayEx& proposedToUse) const override;

private:
    int m_proposed = 0;
};

/** Utility RAAA class to propose some licenses usage. */
class VideoWallLicenseUsageProposer
{
public:
    VideoWallLicenseUsageProposer(
        common::SystemContext* context,
        VideoWallLicenseUsageHelper* helper,
        int screenCount);
    ~VideoWallLicenseUsageProposer();
private:
    QPointer<VideoWallLicenseUsageHelper> m_helper;
    int m_count;
};

} // namespace nx::vms::license
