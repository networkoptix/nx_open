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
    static const QString kUserEnabledAnalyticsEnginesProperty;
    static const QString kCompatibleAnalyticsEnginesProperty;
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

    CameraMediaStreams mediaStreams() const;
    CameraMediaStreamInfo streamInfo(StreamIndex index = StreamIndex::primary) const;

    virtual QnAspectRatio aspectRatio() const;

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
     * Analytics engines (ids) which are actually compatible, enabled and active.
     */
    QSet<QnUuid> enabledAnalyticsEngines() const;

    /**
     * Analytics engines which are actually compatible, enabled and active.
     */
    const nx::vms::common::AnalyticsEngineResourceList enabledAnalyticsEngineResources() const;

    /**
     * Analytics engines (ids) which are explicitly enabled by the user. Not validated against
     * actually active engines or compatible engines.
     */
    QSet<QnUuid> userEnabledAnalyticsEngines() const;

    /**
     * Update analytics engines (ids), which should be used with this camera. Not validated against
     * actually active engines or compatible engines.
     */
    void setUserEnabledAnalyticsEngines(const QSet<QnUuid>& engines);

    /**
     * Analytics engines (ids), which can be potentially used with the camera. Only actually running
     * on the parent server engines are included.
     */
    const QSet<QnUuid> compatibleAnalyticsEngines() const;

    /**
     * Analytics engines, which can be potentially used with the camera. Only actually running
     * on the parent server engines are included.
     */
    nx::vms::common::AnalyticsEngineResourceList compatibleAnalyticsEngineResources() const;

    /**
     * Update analytics engines (ids), which can be potentially used with the camera.
     */
    void setCompatibleAnalyticsEngines(const QSet<QnUuid>& engines);

    /** Engine id to Object type ids. */
    std::map<QnUuid, std::set<QString>> supportedEventTypes() const;

    /** Engine id to Event type ids. */
    std::map<QnUuid, std::set<QString>> supportedObjectTypes() const;

    QHash<QnUuid, QVariantMap> deviceAgentSettingsValues() const;
    void setDeviceAgentSettingsValues(const QHash<QnUuid, QVariantMap>& settingsValues);

    QVariantMap deviceAgentSettingsValues(const QnUuid& engineId) const;
    void setDeviceAgentSettingsValues(const QnUuid& engineId, const QVariantMap& settingsValues);

    std::optional<nx::vms::api::analytics::DeviceAgentManifest> deviceAgentManifest(
        const QnUuid& engineId);

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

    QSet<QnUuid> calculateUserEnabledAnalyticsEngines();

    QSet<QnUuid> calculateCompatibleAnalyticsEngines();

    std::map<QnUuid, std::set<QString>> calculateSupportedEntities(
        ManifestItemIdsFetcher fetcher) const;
    std::map<QnUuid, std::set<QString>> calculateSupportedEventTypes() const;
    std::map<QnUuid, std::set<QString>> calculateSupportedObjectTypes() const;

    DeviceAgentManifestMap fetchDeviceAgentManifests();

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
