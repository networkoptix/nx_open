// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <camera/iomodule/iomodule_monitor.h>
#include <core/ptz/ptz_constants.h>
#include <core/resource/client_resource_fwd.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/vms/api/types/resource_types.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/media/abstract_analytics_metadata_provider.h>
#include <nx/vms/client/core/media/abstract_metadata_consumer_owner.h>
#include <nx/vms/client/core/media/abstract_motion_metadata_provider.h>
#include <nx/vms/client/desktop/analytics/analytics_taxonomy_manager.h>
#include <nx/vms/client/desktop/camera/camera_fwd.h>
#include <nx/vms/license/usage_helper.h>

#include "motion_skip_mask.h"

namespace nx::analytics { class MetadataLogParser; }
namespace nx::analytics::db { struct Filter; }
namespace nx::vms::client::core { class ConsumingMotionMetadataProvider; }

namespace nx::vms::client::desktop {

class WidgetAnalyticsController;

class MediaResourceWidgetPrivate: public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hasVideo MEMBER hasVideo CONSTANT)
    Q_PROPERTY(bool isIoModule MEMBER isIoModule CONSTANT)
    Q_PROPERTY(bool isPlayingLive READ isPlayingLive WRITE setIsPlayingLive NOTIFY stateChanged)
    Q_PROPERTY(bool isOffline READ isOffline WRITE setIsOffline NOTIFY stateChanged)
    Q_PROPERTY(bool isUnauthorized READ isUnauthorized WRITE setIsUnauthorized NOTIFY stateChanged)
    // TODO: should mismatchedCertificate property be added here?
    Q_PROPERTY(nx::vms::license::UsageStatus
        licenseStatus READ licenseStatus NOTIFY licenseStatusChanged)

    using base_type = QObject;
    using AccessController = nx::vms::client::core::AccessController;

public:
    const QnResourcePtr resource;
    const QnMediaResourcePtr mediaResource;
    const QnClientCameraResourcePtr camera;
    const bool hasVideo;
    const bool isIoModule;
    bool isExportedLayout = false;
    bool isPreviewSearchLayout = false;
    bool isAnalyticsSupported = false;
    QnUuid twoWayAudioWidgetId;

    QnIOModuleMonitorPtr ioModuleMonitor;

    bool noExportPermission = false; //< Relevant on for items on an exported layout.

    QScopedPointer<nx::vms::client::core::ConsumingMotionMetadataProvider> motionMetadataProvider;
    nx::vms::client::core::AbstractAnalyticsMetadataProviderPtr analyticsMetadataProvider;

    QScopedPointer<WidgetAnalyticsController> analyticsController;
    std::unique_ptr<nx::analytics::MetadataLogParser> analyticsMetadataLogParser;
    bool analyticsEnabled = false;
    bool analyticsObjectsVisibleForcefully = false;

    mutable nx::utils::ElapsedTimer updateDetailsTimer;
    mutable nx::utils::ElapsedTimer traceFpsTimer;
    mutable QString currentDetailsText;

    const QPointer<nx::vms::client::desktop::analytics::TaxonomyManager> taxonomyManager;

public:
    explicit MediaResourceWidgetPrivate(
        const QnResourcePtr& resource,
        QObject* parent = nullptr);
    virtual ~MediaResourceWidgetPrivate();

    QnResourceDisplayPtr display() const;
    void setDisplay(const QnResourceDisplayPtr& display);

    AccessController* accessController() const;

    bool isPlayingLive() const;
    bool isOffline() const;
    bool isUnauthorized() const;
    bool hasAccess() const;
    bool supportsBasicPtz() const; //< Camera supports Pan, Tilt and Zoom.
    bool supportsPtzCapabilities(Ptz::Capabilities capabilities) const;

    nx::vms::license::UsageStatus licenseStatus() const;

    QSharedPointer<nx::media::AbstractMetadataConsumer> motionMetadataConsumer() const;
    QSharedPointer<nx::media::AbstractMetadataConsumer> analyticsMetadataConsumer() const;

    void setMotionEnabled(bool enabled);

    /** Check is current video stream contains analytics objects metadata. */
    bool isAnalyticsEnabledInStream() const;

    /** Setup current video stream to contain analytics objects metadata. */
    void setAnalyticsEnabledInStream(bool enabled);

    void setAnalyticsFilter(const nx::analytics::db::Filter& value);

    const char* const motionSkipMask(int channel) const;

    qreal getStatisticsFps(int channelCount) const;

signals:
    void stateChanged();
    void licenseStatusChanged();
    void analyticsSupportChanged();

private:
    void updateIsPlayingLive();
    void setIsPlayingLive(bool value);

    void updateIsOffline();
    void setIsOffline(bool value);

    void updateIsUnauthorized();
    void setIsUnauthorized(bool value);

    void updateAccess();
    void setHasAccess(bool value);

    bool calculateIsAnalyticsSupported() const;
    void updateIsAnalyticsSupported();

    void setStreamDataFilter(nx::vms::api::StreamDataFilter filter, bool on);
    void setStreamDataFilters(nx::vms::api::StreamDataFilters filters);

    /** Fill motion skip caches. */
    void ensureMotionSkip() const;

private:
    QnResourceDisplayPtr m_display;
    QScopedPointer<nx::vms::license::SingleCamLicenseStatusHelper> m_licenseStatusHelper;
    const QPointer<AccessController> m_accessController;
    const AccessController::NotifierPtr m_accessNotifier;

    bool m_isPlayingLive = false;
    bool m_isOffline = false;
    bool m_isUnauthorized = false;
    bool m_hasAccess = true;
    bool m_forceDisabledAnalytics = false;

    /** Cache of motion skip masks by channel. */
    mutable std::optional<std::vector<MotionSkipMask>> m_motionSkipMaskCache;
};

} // namespace nx::vms::client::desktop
