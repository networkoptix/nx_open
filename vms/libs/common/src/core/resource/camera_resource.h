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
    static const QString kEnabledAnalyticsEnginesProperty;
    static const QString kDeviceAgentsSettingsValuesProperty;
    static const QString kDeviceAgentManifestsProperty;
    static const QString kSupportedAnalyticsEventsProperty;
    static const QString kSupportedAnalyticsObjectsProperty;

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
    CameraMediaStreamInfo streamInfo(Qn::StreamIndex index = Qn::StreamIndex::primary) const;

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

    const nx::vms::common::AnalyticsEngineResourceList enabledAnalyticsEngineResources() const;
    QSet<QnUuid> enabledAnalyticsEngines() const;
    void setEnabledAnalyticsEngines(const QSet<QnUuid>& engines);

    QHash<QnUuid, QVariantMap> deviceAgentSettingsValues() const;
    void setDeviceAgentSettingsValues(const QHash<QnUuid, QVariantMap>& settingsValues);

    QVariantMap deviceAgentSettingsValues(const QnUuid& engineId) const;
    void setDeviceAgentSettingsValues(const QnUuid& engineId, const QVariantMap& settingsValues);

    std::optional<nx::vms::api::analytics::DeviceAgentManifest> deviceAgentManifest(
        const QnUuid& engineId);

    void setDeviceAgentManifest(
        const QnUuid& engineId,
        const nx::vms::api::analytics::DeviceAgentManifest& manifest);

    virtual void setSupportedAnalyticsEventTypeIds(
        QnUuid engineId, QSet<QString> supportedEvents);

    virtual QMap<QnUuid, QSet<QString>> supportedAnalyticsEventTypeIds() const;
    virtual QSet<QString> supportedAnalyticsEventTypeIds(const QnUuid& engineId) const;

    virtual void setSupportedAnalyticsObjectTypeIds(
        QnUuid engineId, QSet<QString> supportedObjects);

    virtual QMap<QnUuid, QSet<QString>> supportedAnalyticsObjectTypeIds() const;
    virtual QSet<QString> supportedAnalyticsObjectTypeIds(const QnUuid& engineId) const;

signals:
    void ptzCapabilitiesChanged(const QnVirtualCameraResourcePtr& camera);

protected:
    virtual void emitPropertyChanged(const QString& key) override;

private:
    void saveResolutionList( const CameraMediaStreams& supportedNativeStreams );

    void setSupportedAnalyticsItemTypeIds(
        const QString& propertyName,
        QnUuid engineId,
        QSet<QString> supportedItems);

    QMap<QnUuid, QSet<QString>> supportedAnalyticsItemTypeIds(const QString& propertyName) const;
    QSet<QString> supportedAnalyticsItemTypeIds(
        const QString& propertyName,
        const QnUuid& engineId) const;

    QSet<QnUuid> calculateEnabledAnalyticsEngines();

private:
    int m_issueCounter;
    QElapsedTimer m_lastIssueTimer;
    std::map<Qn::ConnectionRole, QString> m_cachedStreamUrls;
    QnMutex m_mediaStreamsMutex;

    CachedValue<QSet<QnUuid>> m_cachedAnalyticsEngines;
    QnMutex m_cacheMutex;
};

const QSize EMPTY_RESOLUTION_PAIR(0, 0);
const QSize SECONDARY_STREAM_DEFAULT_RESOLUTION(480, 360);
const QSize SECONDARY_STREAM_MAX_RESOLUTION(1024, 768);
const QSize UNLIMITED_RESOLUTION(INT_MAX, INT_MAX);

Q_DECLARE_METATYPE(QnVirtualCameraResourcePtr);
Q_DECLARE_METATYPE(QnVirtualCameraResourceList);
