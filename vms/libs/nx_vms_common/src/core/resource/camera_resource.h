// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <set>

#include <QtCore/QElapsedTimer>

#include <core/resource/resource_fwd.h>
#include <core/resource/security_cam_resource.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/lockable.h>
#include <nx/utils/url.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/vms/api/data/device_profile.h>
#include <nx/vms/api/types/rtp_types.h>
#include <nx_ec/ec_api_fwd.h>
#include <utils/common/aspect_ratio.h>

class CameraMediaStreams;
class CameraMediaStreamInfo;
class CameraBitrates;
class CameraBitrateInfo;

class NX_VMS_COMMON_API QnVirtualCameraResource: public QnSecurityCamResource
{
    Q_OBJECT
    Q_FLAGS(Ptz::Capabilities)
    Q_PROPERTY(nx::vms::api::DeviceCapabilities cameraCapabilities
        READ getCameraCapabilities WRITE setCameraCapabilities)
    Q_PROPERTY(Ptz::Capabilities ptzCapabilities
        READ getPtzCapabilities WRITE setPtzCapabilities NOTIFY ptzCapabilitiesChanged)
    using base_type = QnSecurityCamResource;

public:
    static const QString kCompatibleAnalyticsEnginesProperty;
    static const QString kUserEnabledAnalyticsEnginesProperty;
    // This property kept here only because of statistics filtering.
    static const QString kDeviceAgentsSettingsValuesProperty;
    static const QString kDeviceAgentManifestsProperty;
    static const QString kAnalyzedStreamIndexes;
    static const QString kVirtualCameraIgnoreTimeZone;
    static const QString kHttpPortParameterName;
    static const QString kCameraNameParameterName;
    static const QString kUsingOnvifMedia2Type;

    static constexpr nx::vms::api::StreamIndex kDefaultAnalyzedStreamIndex =
        nx::vms::api::StreamIndex::primary;

public:
    QnVirtualCameraResource();

    void forceEnableAudio();
    void forceDisableAudio();
    bool isForcedAudioSupported() const;

    void updateDefaultAuthIfEmpty(const QString& login, const QString& password);

    //! Camera source URL, commonly - rtsp link.
    QString sourceUrl(Qn::ConnectionRole role) const;
    void updateSourceUrl(const nx::utils::Url& url, Qn::ConnectionRole role, bool save = true);

    bool virtualCameraIgnoreTimeZone() const;
    /**
     * Meaningful only for virtual camera.
     * If the value is true, video's timestamps is considered as local client time.
     * Otherwise, timestamps is considered as UTC.
     */
    void setVirtualCameraIgnoreTimeZone(bool value);

    nx::vms::api::RtpTransportType preferredRtpTransport() const;
    CameraMediaStreams mediaStreams() const;
    CameraMediaStreamInfo streamInfo(StreamIndex index = StreamIndex::primary) const;

    /** @return frame aspect ratio of a single channel. Does not account for default rotation. */
    virtual QnAspectRatio aspectRatio() const;

    /** @return frame aspect ratio of a single channel. Accounts for default rotation. */
    virtual QnAspectRatio aspectRatioRotated() const;

    /**
     * @return Ids of Analytics Engines which are actually compatible with the Device, enabled by
     *     the user and active (running on the current server). Includes device-dependent Engines.
     */
    virtual QSet<QnUuid> enabledAnalyticsEngines() const;

    /**
     * @return Analytics Engines which are actually compatible with the Device, enabled by the user
     * and active (running on the current server). Includes device-dependent Engines.
     */
    const nx::vms::common::AnalyticsEngineResourceList enabledAnalyticsEngineResources() const;

    /**
     * @return Ids of Analytics Engines which are explicitly enabled by the user. Not validated for
     *     compatibility with the Device or if the engine is active (running on the current server).
     */
    QSet<QnUuid> userEnabledAnalyticsEngines() const;

    /**
     * Set ids of Analytics Engines which are explicitly enabled by the user. Not validated for
     * compatibility with the Device or if the Engine is active (running on the current server).
     */
    void setUserEnabledAnalyticsEngines(const QSet<QnUuid>& engines);

    /**
     * Set ids of Analytics Engines which are explicitly enabled by the user.
     * This is the same function like 'setUserEnabledAnalyticsEngines' but it returns serialized data instead of storing
     * it to the resource properties.
     */
    nx::vms::api::ResourceParamData serializeUserEnabledAnalyticsEngines(
        const QSet<QnUuid>& engines);

    /**
     * @return Ids of Analytics Engines which can be potentially used with the Device. Only active
     *     (running on the current server) Engines are included.
     */
    virtual const QSet<QnUuid> compatibleAnalyticsEngines() const;

    /**
     * @return Analytics Engines which can be potentially used with the Device. Only active
     *     (running on the current server) Engines are included.
     */
    virtual nx::vms::common::AnalyticsEngineResourceList compatibleAnalyticsEngineResources() const;

    /**
     * Set ids of Analytics Engines which can be potentially used with the Device. Only active
     * (running on the current server) Engines must be included.
     */
    void setCompatibleAnalyticsEngines(const QSet<QnUuid>& engines);

    using AnalyticsEntitiesByEngine = std::map<QnUuid, std::set<QString>>;

    /**
     * Utility function to filter only those entities, which are supported by the provided engines.
     */
    static AnalyticsEntitiesByEngine filterByEngineIds(
        AnalyticsEntitiesByEngine entitiesByEngine,
        const QSet<QnUuid>& engineIds);

    /**
     * @return Map of supported Event types by the Engine id. Only actually compatible with the
     *     Device, enabled by the user and active (running on the current Server) Engines are used.
     */
    virtual AnalyticsEntitiesByEngine supportedEventTypes() const;

    /**
     * @return Map of the supported Object types by the Engine id.
     * @param filterByEngines If true, only actually compatible with the Device, enabled by the
     *     user and active (running on the current Server) Engines are used.
     */
    virtual AnalyticsEntitiesByEngine supportedObjectTypes(
        bool filterByEngines = true) const;

    std::optional<nx::vms::api::analytics::DeviceAgentManifest> deviceAgentManifest(
        const QnUuid& engineId) const;

    void setDeviceAgentManifest(
        const QnUuid& engineId,
        const nx::vms::api::analytics::DeviceAgentManifest& manifest);

    nx::vms::api::StreamIndex analyzedStreamIndex(QnUuid engineId) const;

    void setAnalyzedStreamIndex(QnUuid engineId, nx::vms::api::StreamIndex streamIndex);

    virtual bool hasDualStreamingInternal() const override;

    /** Whether this camera supports web page. */
    bool isWebPageSupported() const;

    /**
     * Customized user-provided port for Camera Web Page.
     * @return positive value if custom port is set; 0 if default port should be used.
     */
    int customWebPagePort() const;

    /** Set custom port for Camera Web Page. Pass 0 to reset custom value. */
    void setCustomWebPagePort(int value);

    Qn::RecordingState recordingState() const;

    /**
     * Checks if motion detection is enabled and actually can work (hardware, or not exceeding
     * resolution limit or is forced).
     */
    bool isMotionDetectionActive() const;

    /**
     * Checks if schedule task is applicable: recording mode and metadata types are supported.
     */
    static bool canApplyScheduleTask(
        const QnScheduleTask& task,
        bool dualStreamingAllowed,
        bool motionDetectionAllowed,
        bool objectDetectionAllowed);

    /**
     * Checks if schedule task is applicable: recording mode and metadata types are supported.
     */
    bool canApplyScheduleTask(const QnScheduleTask& task) const;

    /**
     * Checks if all schedule tasks are applicable.
     */
    static bool canApplySchedule(
        const QnScheduleTaskList& schedule,
        bool dualStreamingAllowed,
        bool motionDetectionAllowed,
        bool objectDetectionAllowed);

    /**
     * Checks if all schedule tasks are applicable.
     */
    bool canApplySchedule(const QnScheduleTaskList& schedule) const;

    static constexpr QSize kMaximumSecondaryStreamResolution{1024, 768};
    static constexpr int kMaximumMotionDetectionPixels
        = kMaximumSecondaryStreamResolution.width() * kMaximumSecondaryStreamResolution.height();

    /**
     * Set a list of available device profiles. It can be used on camera Expert dialog
     * to manually setup camera profile to use
     */
    void setAvailableProfiles(const nx::vms::api::DeviceProfiles& value);

    /**
     * Get a list of available device profiles.
     */
    nx::vms::api::DeviceProfiles availableProfiles() const;

    /**
     * Manually setup a profile ID to use.
     */
    void setForcedProfile(const QString& id, nx::vms::api::StreamIndex index);

    /**
     * Read manually configured profile ID to use. Can be empty.
     */
    QString forcedProfile(nx::vms::api::StreamIndex index) const;

    static QSet<QnUuid> calculateUserEnabledAnalyticsEngines(const QString& value);

    using DeviceAgentManifestMap = std::map<QnUuid, nx::vms::api::analytics::DeviceAgentManifest>;
    DeviceAgentManifestMap deviceAgentManifests() const;

signals:
    void ptzCapabilitiesChanged(const QnVirtualCameraResourcePtr& camera);
    void userEnabledAnalyticsEnginesChanged(const QnVirtualCameraResourcePtr& camera);
    void compatibleAnalyticsEnginesChanged(const QnVirtualCameraResourcePtr& camera);
    void deviceAgentManifestsChanged(const QnVirtualCameraResourcePtr& camera);
    void isIOModuleChanged(const QnVirtualCameraResourcePtr& camera);

    // TODO: Get rid of this "maybe" logic.
    void compatibleEventTypesMaybeChanged(const QnVirtualCameraResourcePtr& camera);
    void compatibleObjectTypesMaybeChanged(const QnVirtualCameraResourcePtr& camera);

protected:
    virtual void emitPropertyChanged(
        const QString& key, const QString& prevValue, const QString& newValue) override;

private:
    using ManifestItemIdsFetcher =
        std::function<std::set<QString>(const nx::vms::api::analytics::DeviceAgentManifest&)>;

private:
    QSet<QnUuid> calculateUserEnabledAnalyticsEngines() const;

    QSet<QnUuid> calculateCompatibleAnalyticsEngines() const;

    std::map<QnUuid, std::set<QString>> calculateSupportedEntities(
        ManifestItemIdsFetcher fetcher) const;
    std::map<QnUuid, std::set<QString>> calculateSupportedEventTypes() const;
    std::map<QnUuid, std::set<QString>> calculateSupportedObjectTypes() const;


    std::optional<nx::vms::api::StreamIndex> obtainUserChosenAnalyzedStreamIndex(
        QnUuid engineId) const;
    nx::vms::api::StreamIndex analyzedStreamIndexInternal(const QnUuid& engineId) const;

private:
    nx::utils::CachedValue<QSet<QnUuid>> m_cachedUserEnabledAnalyticsEngines;
    nx::utils::CachedValue<QSet<QnUuid>> m_cachedCompatibleAnalyticsEngines;
    nx::utils::CachedValue<DeviceAgentManifestMap> m_cachedDeviceAgentManifests;
    nx::utils::CachedValue<std::map<QnUuid, std::set<QString>>> m_cachedSupportedEventTypes;
    nx::utils::CachedValue<std::map<QnUuid, std::set<QString>>> m_cachedSupportedObjectTypes;
    mutable nx::utils::Lockable<std::map<QnUuid, nx::vms::api::StreamIndex>> m_cachedAnalyzedStreamIndex;
};

constexpr QSize EMPTY_RESOLUTION_PAIR(0, 0);
constexpr QSize SECONDARY_STREAM_MAX_RESOLUTION =
    QnVirtualCameraResource::kMaximumSecondaryStreamResolution;
constexpr QSize UNLIMITED_RESOLUTION(INT_MAX, INT_MAX);
