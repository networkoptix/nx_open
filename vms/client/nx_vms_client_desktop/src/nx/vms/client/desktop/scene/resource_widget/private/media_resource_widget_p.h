#pragma once

#include <core/resource/client_resource_fwd.h>

#include <licensing/license_fwd.h>

#include <nx/client/core/media/abstract_motion_metadata_provider.h>
#include <nx/client/core/media/abstract_analytics_metadata_provider.h>
#include <nx/client/core/media/abstract_metadata_consumer_owner.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/vms/client/desktop/camera/camera_fwd.h>
#include <nx/vms/api/types/resource_types.h>

#include <utils/common/connective.h>

class QnSingleCamLicenseStatusHelper;
class QnWorkbenchAccessController;

namespace nx::analytics { class MetadataLogParser; }
namespace nx::analytics::db { struct Filter; }

namespace nx::vms::client::desktop {

class WidgetAnalyticsController;

class MediaResourceWidgetPrivate: public Connective<QObject>
{
    Q_OBJECT
    Q_PROPERTY(bool hasVideo MEMBER hasVideo CONSTANT)
    Q_PROPERTY(bool isIoModule MEMBER isIoModule CONSTANT)
    Q_PROPERTY(bool isPlayingLive READ isPlayingLive WRITE setIsPlayingLive NOTIFY stateChanged)
    Q_PROPERTY(bool isOffline READ isOffline WRITE setIsOffline NOTIFY stateChanged)
    Q_PROPERTY(bool isUnauthorized READ isUnauthorized WRITE setIsUnauthorized NOTIFY stateChanged)
    Q_PROPERTY(QnLicenseUsageStatus licenseStatus READ licenseStatus NOTIFY licenseStatusChanged)

    using base_type = Connective<QObject>;

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

    QScopedPointer<nx::vms::client::core::AbstractMotionMetadataProvider> motionMetadataProvider;
    nx::vms::client::core::AbstractAnalyticsMetadataProviderPtr analyticsMetadataProvider;

    QScopedPointer<WidgetAnalyticsController> analyticsController;
    std::unique_ptr<nx::analytics::MetadataLogParser> analyticsMetadataLogParser;

    mutable nx::utils::ElapsedTimer updateDetailsTimer;
    mutable QString currentDetailsText;

public:
    explicit MediaResourceWidgetPrivate(
        const QnResourcePtr& resource,
        QnWorkbenchAccessController* accessController,
        QObject* parent = nullptr);
    virtual ~MediaResourceWidgetPrivate();

    QnResourceDisplayPtr display() const;
    void setDisplay(const QnResourceDisplayPtr& display);

    bool isPlayingLive() const;
    bool isOffline() const;
    bool isUnauthorized() const;
    bool canControlPtz() const;

    QnLicenseUsageStatus licenseStatus() const;

    QSharedPointer<nx::media::AbstractMetadataConsumer> motionMetadataConsumer() const;
    QSharedPointer<nx::media::AbstractMetadataConsumer> analyticsMetadataConsumer() const;

    void setMotionEnabled(bool enabled);

    /** Check is current video stream contains analytics objects metadata. */
    bool isAnalyticsEnabledInStream() const;

    /** Setup current video stream to contain analytics objects metadata. */
    void setAnalyticsEnabledInStream(bool enabled);

    void setAnalyticsFilter(const nx::analytics::db::Filter& value);

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

    bool calculateIsAnalyticsSupported() const;
    void updateIsAnalyticsSupported();

    /** Request analytics objects once to check if they exist in the archive. */
    void requestAnalyticsObjectsExistence();

    void setStreamDataFilter(nx::vms::api::StreamDataFilter filter, bool on);
    void setStreamDataFilters(nx::vms::api::StreamDataFilters filters);

private:
    QnResourceDisplayPtr m_display;
    QScopedPointer<QnSingleCamLicenseStatusHelper> m_licenseStatusHelper;
    QPointer<QnWorkbenchAccessController> m_accessController;

    bool m_isPlayingLive = false;
    bool m_isOffline = false;
    bool m_isUnauthorized = false;
    bool m_analyticsObjectsFound = false;
    bool m_forceDisabledAnalytics = false;
};

} // namespace nx::vms::client::desktop
