#pragma once

#include <deque>
#include <set>

#include <QtCore/QMetaType>
#include <QtCore/QElapsedTimer>

#include <nx_ec/ec_api_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <utils/camera/camera_diagnostics.h>
#include <utils/common/aspect_ratio.h>

#include <core/resource/camera_media_stream_info.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/security_cam_resource.h>
#include <nx/utils/url.h>

#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/vms/api/types/rtp_types.h>

class CameraMediaStreams;
class CameraBitrates;
class CameraBitrateInfo;

class QnVirtualCameraResource : public QnSecurityCamResource
{
    Q_OBJECT
    Q_FLAGS(Qn::CameraCapabilities)
    Q_FLAGS(Ptz::Capabilities)
    Q_PROPERTY(Qn::CameraCapabilities cameraCapabilities
        READ getCameraCapabilities WRITE setCameraCapabilities)
    Q_PROPERTY(Ptz::Capabilities ptzCapabilities
        READ getPtzCapabilities WRITE setPtzCapabilities NOTIFY ptzCapabilitiesChanged)
    using base_type = QnSecurityCamResource;

public:
    static const QString kCompatibleAnalyticsEnginesProperty;
    static const QString kUserEnabledAnalyticsEnginesProperty;
    static const QString kDeviceAgentsSettingsValuesProperty;
    static const QString kDeviceAgentManifestsProperty;

public:
    QnVirtualCameraResource(QnCommonModule* commonModule = nullptr);

    virtual QString getUniqueId() const override;

    void forceEnableAudio();
    void forceDisableAudio();
    bool isForcedAudioSupported() const;
    virtual int saveAsync();
    void updateDefaultAuthIfEmpty(const QString& login, const QString& password);

    //! Camera source URL, commonly - rtsp link.
    QString sourceUrl(Qn::ConnectionRole role) const;
    void updateSourceUrl(const nx::utils::Url& url, Qn::ConnectionRole role, bool save = true);

    static int issuesTimeoutMs();

    void issueOccured();
    void cleanCameraIssues();

    nx::vms::api::RtpTransportType preferredRtpTransport() const;
    CameraMediaStreams mediaStreams() const;
    CameraMediaStreamInfo streamInfo(StreamIndex index = StreamIndex::primary) const;

    /** @return frame aspect ratio of a single channel. Does not account for default rotation. */
    virtual QnAspectRatio aspectRatio() const;

    /** @return frame aspect ratio of a single channel. Accounts for default rotation. */
    virtual QnAspectRatio aspectRatioRotated() const;

    // TODO: saveMediaStreamInfoIfNeeded and saveBitrateIfNeeded should be moved into
    // nx::vms::server::resource::Camera, as soon as QnLiveStreamProvider moved into nx::vms::server.

    /** @return true if mediaStreamInfo differs from existing and has been saved. */
    bool saveMediaStreamInfoIfNeeded( const CameraMediaStreamInfo& mediaStreamInfo );
    bool saveMediaStreamInfoIfNeeded( const CameraMediaStreams& streams );

    /** @return true if bitrateInfo.encoderIndex is not already saved. */
    bool saveBitrateIfNeeded( const CameraBitrateInfo& bitrateInfo );

    // TODO: Move to nx::vms::server::resource::Camera, it should be used only on server cameras!
    /**
     * Returns advanced live stream parameters. These parameters are configured on advanced tab.
     * For primary stream this parameters are merged with parameters on record schedule.
     */
    virtual QnAdvancedStreamParams advancedLiveStreamParams() const;

    /**
     * @return Ids of Analytics Engines which are actually compatible with the Device, enabled by
     * the user and active (running on the current server).
     */
    QSet<QnUuid> enabledAnalyticsEngines() const;

    /**
     * @return Analytics Engines which are actually compatible with the Device, enabled by the user
     * and active (running on the current server).
     */
    const nx::vms::common::AnalyticsEngineResourceList enabledAnalyticsEngineResources() const;

    /**
     * @return Ids of Analytics Engines which are explicitly enabled by the user. Not validated for
     * compatibility with the Device or if the engine is active (running on the current server).
     */
    QSet<QnUuid> userEnabledAnalyticsEngines() const;

    /**
     * Set ids of Analytics Engines which are explicitly enabled by the user. Not validated for
     * compatibility with the Device or if the engine is active (running on the current server).
     */
    void setUserEnabledAnalyticsEngines(const QSet<QnUuid>& engines);

    /**
     * @return Ids of Analytics Engines, which can be potentially used with the Device. Only active
     * (running on the current server) Engines are included.
     */
    const QSet<QnUuid> compatibleAnalyticsEngines() const;

    /**
     * @return Analytics Engines, which can be potentially used with the Device. Only active
     * (running on the current server) Engines are included.
     */
    nx::vms::common::AnalyticsEngineResourceList compatibleAnalyticsEngineResources() const;

    /**
     * Set ids of Analytics Engines, which can be potentially used with the Device. Only active
     * (running on the current server) Engines must be included.
     */
    void setCompatibleAnalyticsEngines(const QSet<QnUuid>& engines);

    /**
     * @return Map of supported Event types by the Engine id. Only actually compatible with the
     * Device, enabled by the user and active (running on the current Server) Engines are used.
     */
    std::map<QnUuid, std::set<QString>> supportedEventTypes() const;

    /**
     * @return Map of supported Object types by the Engine id. Only actually compatible with the
     * Device, enabled by the user and active (running on the current Server) Engines are used.
     */
    std::map<QnUuid, std::set<QString>> supportedObjectTypes() const;

    QHash<QnUuid, QVariantMap> deviceAgentSettingsValues() const;
    void setDeviceAgentSettingsValues(const QHash<QnUuid, QVariantMap>& settingsValues);

    QVariantMap deviceAgentSettingsValues(const QnUuid& engineId) const;
    void setDeviceAgentSettingsValues(const QnUuid& engineId, const QVariantMap& settingsValues);

    std::optional<nx::vms::api::analytics::DeviceAgentManifest> deviceAgentManifest(
        const QnUuid& engineId) const;

    void setDeviceAgentManifest(
        const QnUuid& engineId,
        const nx::vms::api::analytics::DeviceAgentManifest& manifest);

signals:
    void ptzCapabilitiesChanged(const QnVirtualCameraResourcePtr& camera);
    void userEnabledAnalyticsEnginesChanged(const QnVirtualCameraResourcePtr& camera);
    void compatibleAnalyticsEnginesChanged(const QnVirtualCameraResourcePtr& camera);
    void deviceAgentManifestsChanged(const QnVirtualCameraResourcePtr& camera);

protected:
    virtual void emitPropertyChanged(const QString& key) override;

private:
    using DeviceAgentManifestMap = std::map<QnUuid, nx::vms::api::analytics::DeviceAgentManifest>;
    using ManifestItemIdsFetcher =
        std::function<std::set<QString>(const nx::vms::api::analytics::DeviceAgentManifest&)>;

private:
    void saveResolutionList( const CameraMediaStreams& supportedNativeStreams );

    QSet<QnUuid> calculateUserEnabledAnalyticsEngines() const;

    QSet<QnUuid> calculateCompatibleAnalyticsEngines() const;

    std::map<QnUuid, std::set<QString>> calculateSupportedEntities(
        ManifestItemIdsFetcher fetcher) const;
    std::map<QnUuid, std::set<QString>> calculateSupportedEventTypes() const;
    std::map<QnUuid, std::set<QString>> calculateSupportedObjectTypes() const;

    DeviceAgentManifestMap fetchDeviceAgentManifests() const;

private:
    int m_issueCounter;
    QElapsedTimer m_lastIssueTimer;
    std::map<Qn::ConnectionRole, QString> m_cachedStreamUrls;
    QnMutex m_mediaStreamsMutex;

    CachedValue<QSet<QnUuid>> m_cachedUserEnabledAnalyticsEngines;
    CachedValue<QSet<QnUuid>> m_cachedCompatibleAnalyticsEngines;
    CachedValue<DeviceAgentManifestMap> m_cachedDeviceAgentManifests;
    CachedValue<std::map<QnUuid, std::set<QString>>> m_cachedSupportedEventTypes;
    CachedValue<std::map<QnUuid, std::set<QString>>> m_cachedSupportedObjectTypes;

    mutable QnMutex m_cacheMutex;
};

const QSize EMPTY_RESOLUTION_PAIR(0, 0);
const QSize SECONDARY_STREAM_DEFAULT_RESOLUTION(480, 360);
const QSize SECONDARY_STREAM_MAX_RESOLUTION(1024, 768);
const QSize UNLIMITED_RESOLUTION(INT_MAX, INT_MAX);

Q_DECLARE_METATYPE(QnVirtualCameraResourcePtr);
Q_DECLARE_METATYPE(QnVirtualCameraResourceList);
