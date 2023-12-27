// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/model/api_ioport_data.h>
#include <common/common_globals.h>
#include <core/dataprovider/live_stream_params.h>
#include <core/misc/schedule_task.h>
#include <core/resource/media_resource.h>
#include <core/resource/network_resource.h>
#include <nx/core/resource/using_media2_type.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/value_cache.h>
#include <nx/vms/api/data/device_model.h>
#include <nx/vms/api/data/media_stream_capability.h>
#include <nx/vms/common/resource/camera_hotspots_data.h>
#include <nx/vms/common/resource/remote_archive_types.h>
#include <nx/vms/event/event_fwd.h>
#include <recording/time_period_list.h>

class QnAbstractArchiveDelegate;
class QnResourceData;
class QnMotionRegion;

namespace nx::vms::rules { struct NetworkIssueInfo; }
namespace nx::core::resource { class AbstractRemoteArchiveManager; }
namespace nx::media { enum class StreamEvent; }
namespace nx::core::ptz { enum class PresetType; }

class NX_VMS_COMMON_API QnSecurityCamResource: public QnNetworkResource
{
    Q_OBJECT
    using base_type = QnNetworkResource;

protected:
    using MotionType = nx::vms::api::MotionType;
    using MotionTypes = nx::vms::api::MotionTypes;

public:
    static constexpr int kDefaultMaxFps = 15;
    static QnUuid makeCameraIdFromPhysicalId(const QString& physicalId);

    using StreamIndex = nx::vms::api::StreamIndex;

    struct NX_VMS_COMMON_API MotionStreamIndex
    {
        StreamIndex index = StreamIndex::undefined;
        bool isForced = false;

        bool operator==(const MotionStreamIndex& other) const;
        bool operator!=(const MotionStreamIndex& other) const;
    };

    enum class StreamFpsSharingMethod
    {
        /**
         * If second stream is running whatever fps it has, first stream can get
         * `maximumFps - secondstreamFps`.
         */
        basic,

        /**
         * If second stream is running whatever megapixel it has, first stream can get
         * maxMegapixels - secondstreamPixels.
         */
        pixels,

        /** Second stream does not affect first stream's fps. */
        none,
    };

public:
    QnSecurityCamResource();
    virtual ~QnSecurityCamResource();

    QnMediaServerResourcePtr getParentServer() const;

    bool isAudioSupported() const;
    bool isIOModule() const;
    int motionWindowCount() const;
    int motionMaskWindowCount() const;
    int motionSensWindowCount() const;
    bool hasTwoWayAudio() const;

    /**
     * Actual motion type used when the `MotionType::default_` value is selected.
     */
    MotionType getDefaultMotionType() const;

    /**
     * Flags set for all motion types, supported by the Camera driver.
     */
    MotionTypes supportedMotionTypes() const;

    /**
     * If Camera supports motion detection, e.g. it has enabled dual streaming or forced motion
     * detection on primary stream or uses hardware motion detection.
     */
    bool isMotionDetectionSupported() const;

    /**
     * If motion detection is supported and actually enabled by the User.
     */
    bool isMotionDetectionEnabled() const;

    /**
     * Actually used motion detection type. `MotionType::MT_None` if User disabled motion
     * detection.
     */
    virtual MotionType getMotionType() const;

    /**
     * Select preferred motion detection type. `MotionType::MT_None` should be passed to disable
     * motion detection.
     */
    void setMotionType(MotionType value);

    virtual int getMaxFps(StreamIndex streamIndex = StreamIndex::primary) const;

    virtual void setMaxFps(int fps, StreamIndex streamIndex = StreamIndex::primary);

    virtual int reservedSecondStreamFps() const;

    QList<QnMotionRegion> getMotionRegionList() const;
    void setMotionRegionList(const QList<QnMotionRegion>& maskList);

    QnMotionRegion getMotionRegion() const;

    /** Returns which stream should be used for motion detection and whether it is forced. */
    virtual MotionStreamIndex motionStreamIndex() const;
    MotionStreamIndex motionStreamIndexInternal() const;

    /**
     * Enable forced motion detection on a selected stream or switch to automatic mode.
     */
    void setMotionStreamIndex(MotionStreamIndex value);

    /**
     * If motion detection in the remote archive is enabled. Actual for the virtual
     * cameras and for the edge cameras (with RemoteArchiveCapability).
     */
    bool isRemoteArchiveMotionDetectionEnabled() const;

    /**
     * Enable or disable motion detection in the remote archive. Actual for the virtual
     * cameras and for the edge cameras (with RemoteArchiveCapability).
     */
    void setRemoteArchiveMotionDetectionEnabled(bool value);

    void setScheduleTasks(const QnScheduleTaskList &scheduleTasks);
    QnScheduleTaskList getScheduleTasks() const;

    /** @return Whether dual streaming is supported by the camera internally. */
    virtual bool hasDualStreamingInternal() const;

    /**
     * @return Whether dual streaming is supported by the camera and is not forcefully disabled by
     * a user (@see `isDualStreamingDisabled()`).
     */
    virtual bool hasDualStreaming() const;

    /** Returns true if camera stores archive on a external system */
    bool isDtsBased() const;

    /** @return True if recording schedule can be configured for this device. */
    bool supportsSchedule() const;

    /** Returns true if it is a analog camera */
    bool isAnalog() const;

    /** Returns true if it is a analog encoder (described in resource_data.json) */
    bool isAnalogEncoder() const;

    /** Returns true if it is an onvif device camera. */
    bool isOnvifDevice() const;

    /** Returns edge, analog or digital class */
    virtual Qn::LicenseType licenseType() const;

    /**
     * Returns true if all cameras in a same camera group should share 1 license
     */
    bool isSharingLicenseInGroup() const;

    bool isNvr() const;

    bool isMultiSensorCamera() const;

    static nx::vms::api::DeviceType calculateDeviceType(
        bool isDtsBased,
        bool isIoModule,
        bool isAnalogEncoder,
        bool isAnalog,
        bool isGroupIdEmpty,
        bool hasVideo,
        bool hasAudioCapability);
    void setDeviceType(nx::vms::api::DeviceType);
    nx::vms::api::DeviceType deviceType() const;

    virtual StreamFpsSharingMethod streamFpsSharingMethod() const;
    void setStreamFpsSharingMethod(StreamFpsSharingMethod value);

    // TODO: move this flags inside CameraMediaCapability struct
    nx::vms::api::DeviceCapabilities getCameraCapabilities() const;
    bool hasCameraCapabilities(nx::vms::api::DeviceCapabilities capabilities) const;
    void setCameraCapabilities(nx::vms::api::DeviceCapabilities capabilities);
    void setCameraCapability(nx::vms::api::DeviceCapability capability, bool value);

    nx::vms::api::CameraMediaCapability cameraMediaCapability() const;
    void setCameraMediaCapability(const nx::vms::api::CameraMediaCapability& value);

    bool isRecordingEventAttached() const;

    virtual QnAbstractArchiveDelegate* createArchiveDelegate() { return 0; }
    virtual QnAbstractStreamDataProvider* createArchiveDataProvider() { return 0; }

    //!Returns user-defined camera name (if not empty), default name otherwise
    QString getUserDefinedName() const;

    //!Returns user-defined group name (if not empty) or default group name
    virtual QString getUserDefinedGroupName() const;
    //!Returns default group name
    QString getDefaultGroupName() const;
    virtual void setDefaultGroupName(const QString& value);
    //!Set group name (the one is show to the user in client)
    /*!
        This name is set by user.
        \a setDefaultGroupName name is generally set automatically (e.g., by server)
    */
    void setUserDefinedGroupName( const QString& value );
    virtual QString getGroupId() const;
    virtual void setGroupId(const QString& value);

    virtual QString getSharedId() const;

    /** Check if a license is used for the current camera. */
    bool isScheduleEnabled() const;
    void setScheduleEnabled(bool value);

    Qn::FailoverPriority failoverPriority() const;
    void setFailoverPriority(Qn::FailoverPriority value);

    void updateAudioRequired();
    void updateAudioRequiredOnDevice(const QString& deviceId);

    /**
     * @returns True if the audio required by one of the:
     * - audio enabled in client
     * - audio forced
     * - audio mapped to another device
     * this getter should be used by data providers to decide whether to retrieve audio from source
     * or not
     */
    bool isAudioRequired() const;

    /**
     * @returns True if the audio should not be disabled
     */
    bool isAudioForced() const;

    /**
     * @returns True if the audio receiving is enabled. Enabled state doesn't guarantee
     *     availability of actual audio stream since audio input may be not configured right.
     */
    bool isAudioEnabled() const;

    /**
     * Sets whether audio receiving (but not transmitting) should be enabled or disabled.
     * If <tt>isAudioForced()</tt> returns true, audio receiving is implicitly enabled and this
     * method has no effect.
     */
    void setAudioEnabled(bool enabled);

    /**
     * @return ID of device which used as audio input override for the camera.
     */
    QnUuid audioInputDeviceId() const;

    /**
     * Sets ID of device which will be used as audio input override for the camera.
     * @param deviceId Valid another device ID expected. Null ID should be set if this device's
     *     own audio input is intended to be used.
     */
    void setAudioInputDeviceId(const QnUuid& deviceId);

    /**
     * @returns True if the audio transmitting is enabled. Enabled state doesn't guarantee
     *     actual transmission possibility since audio output may be not configured right.
     */
    bool isTwoWayAudioEnabled() const;

    /**
     * Sets whether audio transmitting should be enabled or disabled.
     */
    void setTwoWayAudioEnabled(bool value);

    /**
     * @return ID of device which used as audio output override for the camera.
     */
    QnUuid audioOutputDeviceId() const;

    /**
     * @return Redirected audio output (if any) or self.
     */
    QnSecurityCamResourcePtr audioOutputDevice() const;

    /**
     * Sets ID of device which will be used as audio output override for the camera.
     * @param deviceId Valid another device ID expected. Null ID should be set if this device's
     *     own audio output is intended to be used.
     */
    void setAudioOutputDeviceId(const QnUuid& deviceId);

    bool isManuallyAdded() const;
    void setManuallyAdded(bool value);

    nx::vms::api::CameraBackupQuality getBackupQuality() const;
    void setBackupQuality(nx::vms::api::CameraBackupQuality value);

    /** Get backup qualities, substantiating default value. */
    nx::vms::api::CameraBackupQuality getActualBackupQualities() const;

    /**
     * @return Whether camera hotspots enabled or not.
     */
    bool cameraHotspotsEnabled() const;

    /**
     * Sets whether camera hotspots enabled or not.
     */
    void setCameraHotspotsEnabled(bool enabled);

    /**
     * @return List of camera hotspots description data structures.
     */
    nx::vms::common::CameraHotspotDataList cameraHotspots() const;

    /**
     * Sets camera hotspots descriptions to the resource as is. Data validity is the responsibility
     * of the caller.
     */
    void setCameraHotspots(const nx::vms::common::CameraHotspotDataList& cameraHotspots);

    QString getModel() const;
    void setModel(const QString &model);

    QString getFirmware() const;
    void setFirmware(const QString &firmware);

    bool trustCameraTime() const;
    void setTrustCameraTime(bool value);

    bool keepCameraTimeSettings() const;
    void setKeepCameraTimeSettings(bool value);

    QString getVendor() const;
    void setVendor(const QString &value);

    virtual int logicalId() const override;
    virtual void setLogicalId(int value) override;

    bool isGroupPlayOnly() const;

    bool needsToChangeDefaultPassword() const;

    //!Implementation of QnMediaResource::toResource
    virtual const QnResource* toResource() const override;
    //!Implementation of QnMediaResource::toResource
    virtual QnResource* toResource() override;
    //!Implementation of QnMediaResource::toResource
    virtual const QnResourcePtr toResourcePtr() const override;
    //!Implementation of QnMediaResource::toResource
    virtual QnResourcePtr toResourcePtr() override;

    /**
     * @return Whether dual streaming is disabled by a user. Second stream is not provided by the
     * camera in this case, so it cannot be used for motion detection.
    */
    bool isDualStreamingDisabled() const;

    /**
     * Completely disable dual streaming on the camera side. Second stream will not be not provided
     * in this case, so it cannot be used for motion detection.
    */
    void setDisableDualStreaming(bool value);

    /**
     * @return Whether primary stream is not recorded by the server. Even if recording is
     * disabled, the stream still can be used for motion or objects detection.
     */
    bool isPrimaryStreamRecorded() const;

    /**
     * Enable or disable primary stream recording. Even if recording is disabled, the stream still
     * can be used for motion or objects detection.
     */
    void setPrimaryStreamRecorded(bool value);

    /**
     * @return Whether primary and secondary streams should record audio track.
     */
    bool isAudioRecorded() const;

    /**
     * Enable or disable audio track recording for the primary and secondary streams.
     */
    void setRecordAudio(bool value);

    /**
     * @return Whether secondary stream is not recorded by the server. Even if recording is
     * disabled, the stream still can be used for motion or objects detection.
     */
    bool isSecondaryStreamRecorded() const;

    /**
     * Enable or disable secondary stream recording. Even if recording is disabled, the stream
     * still can be used for motion or objects detection.
     */
    void setSecondaryStreamRecorded(bool value);

    void setCameraControlDisabled(bool value);
    bool isCameraControlDisabledInternal() const;
    virtual bool isCameraControlDisabled() const;

    // TODO: #2.4 #rvasilenko #High Move to runtime data
    Qn::CameraStatusFlags statusFlags() const;
    bool hasStatusFlags(Qn::CameraStatusFlag value) const;
    void setStatusFlags(Qn::CameraStatusFlags value);
    void addStatusFlags(Qn::CameraStatusFlag value);
    void removeStatusFlags(Qn::CameraStatusFlag value);

    virtual bool needCheckIpConflicts() const;

    void setMaxPeriod(std::chrono::seconds value);
    std::chrono::seconds maxPeriod() const;

    void setMinPeriod(std::chrono::seconds value);
    std::chrono::seconds minPeriod() const;

    int recordBeforeMotionSec() const;
    void setRecordBeforeMotionSec(int value);

    int recordAfterMotionSec() const;
    void setRecordAfterMotionSec(int value);

    void setPreferredServerId(const QnUuid& value);
    QnUuid preferredServerId() const;

    void setRemoteArchiveSynchronizationMode(
        nx::vms::common::RemoteArchiveSyncronizationMode mode);

    nx::vms::common::RemoteArchiveSyncronizationMode
        getRemoteArchiveSynchronizationMode() const;

    void setManualRemoteArchiveSynchronizationTriggered(bool isTriggered = true);
    bool isManualRemoteArchiveSynchronizationTriggered() const;

    /**
     * If preferred server is not set, assigns current server as preferred.
     */
    void updatePreferredServerId();

    //!Returns list of time periods of DTS archive, containing motion at specified \a regions with timestamp in region [\a msStartTime; \a msEndTime)
    /*!
        \param detailLevel Minimal time period gap (usec) that is of interest to the caller.
            Two time periods lying closer to each other than \a detailLevel usec SHOULD be reported as one
        \note Used only if \a QnSecurityCamResource::isDtsBased() is \a true
        \note Default implementation does nothing
    */
    virtual QnTimePeriodList getDtsTimePeriodsByMotionRegion(
        const QList<QRegion>& regions,
        qint64 msStartTime,
        qint64 msEndTime,
        int detailLevel,
        bool keepSmalChunks,
        int limit,
        Qt::SortOrder sortOrder);

    // in some cases I just want to update couple of field from just discovered resource
    virtual bool mergeResourcesIfNeeded(const QnNetworkResourcePtr &source) override;

    /**
     * @param ports Ports data to save into resource property.
     * @param needMerge If true, overrides new ports description with saved state, because it might
     *      be edited by client. TODO: This should be fixed by using different properties!
     * @return true Merge has happened, false otherwise.
     */
    virtual bool setIoPortDescriptions(QnIOPortDataList ports, bool needMerge);

    /**
     * @param type Filters ports by type, does not filter if Qn::PT_Unknown.
     */
    QnIOPortDataList ioPortDescriptions(Qn::IOPortType type = Qn::PT_Unknown) const;

    nx::vms::api::ExtendedCameraOutputs extendedOutputs() const;

    virtual bool useBitratePerGop() const;

    virtual nx::core::resource::UsingOnvifMedia2Type useMedia2ToFetchProfiles() const;

    // Allow getting multi video layout directly from a RTSP SDP info
    virtual bool allowRtspVideoLayout() const { return true; }

    /**
     * Return non zero media event error if camera resource has an issue.
     */
    nx::media::StreamEvent checkForErrors() const;

    virtual nx::core::resource::AbstractRemoteArchiveManager* remoteArchiveManager();

    virtual float suggestBitrateKbps(
        const QnLiveStreamParams& streamParams, Qn::ConnectionRole role) const;
    static float rawSuggestBitrateKbps(
        Qn::StreamQuality quality, QSize resolution, int fps, const QString& codec);

    /**
     * All events emitted by analytics driver bound to the resource can be captured within
     * this method.
     * @return True if event has been captured, false otherwise.
     */
    virtual bool captureEvent(const nx::vms::event::AbstractEventPtr& event);

    /**
     * @return Id of the analytics Event type that corresponds to eventType if Events of eventType
     *     are obtained via Analytics Plugin, empty string otherwise.
     */
    virtual QString vmsToAnalyticsEventTypeId(nx::vms::api::EventType eventType) const;

    virtual bool hasVideo(const QnAbstractStreamDataProvider* dataProvider = nullptr) const override;

    /**
     * Update user password at the camera. This function is able to change password for existing user only.
     */
    virtual bool setCameraCredentialsSync(
        const QAuthenticator& auth, QString* outErrorString = nullptr);

    /**
     * Returns true if camera credential was auto detected by media server.
     */
    bool isDefaultAuth() const;

    virtual int suggestBitrateForQualityKbps(Qn::StreamQuality q, QSize resolution, int fps,
        const QString& codec, Qn::ConnectionRole role = Qn::CR_Default) const;

    static Qn::ConnectionRole toConnectionRole(StreamIndex index);
    static StreamIndex toStreamIndex(Qn::ConnectionRole role);

    nx::core::ptz::PresetType preferredPtzPresetType() const;

    nx::core::ptz::PresetType userPreferredPtzPresetType() const;
    void setUserPreferredPtzPresetType(nx::core::ptz::PresetType);

    nx::core::ptz::PresetType defaultPreferredPtzPresetType() const;
    void setDefaultPreferredPtzPresetType(nx::core::ptz::PresetType);

    Ptz::Capabilities ptzCapabilitiesUserIsAllowedToModify() const;

    void setPtzCapabilitesUserIsAllowedToModify(Ptz::Capabilities);

    Ptz::Capabilities ptzCapabilitiesAddedByUser() const;

    void setPtzCapabilitiesAddedByUser(Ptz::Capabilities capabilities);

    QPointF ptzPanTiltSensitivity() const;
    bool isPtzPanTiltSensitivityUniform() const; //< Whether pan and tilt use the same sensitivity.
    void setPtzPanTiltSensitivity(const QPointF& value);
    void setPtzPanTiltSensitivity(const qreal& uniformValue);

    bool isVideoQualityAdjustable() const;

    virtual QnResourceData resourceData() const;
    nx::vms::api::BackupContentTypes getBackupContentType() const;
    void setBackupContentType(nx::vms::api::BackupContentTypes contentTypes);

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() = 0;

    /**
     * @return backup policy of the device. Besides the values corresponding to On and Off state,
     *     it may be equal to the <tt>nx::vms::api::BackupPolicy::byDefault</tt> value, which means
     *     that actual backup enabled state will be taken from the global backup settings.
     */
    nx::vms::api::BackupPolicy getBackupPolicy() const;
    void setBackupPolicy(nx::vms::api::BackupPolicy backupPolicy);

    /**
     * @return whether device should be backed up or not. Value is deduced from the backup policy
     *     and additionally, from the global backup settings for the case when backup policy is
     *     <tt>nx::vms::api::BackupPolicy::byDefault</tt>.
     */
    bool isBackupEnabled() const;

    virtual QString getUrl() const override;

    nx::vms::api::CameraAttributesData getUserAttributes() const;
    void setUserAttributes(const nx::vms::api::CameraAttributesData& attributes);
    void setUserAttributesAndNotify(const nx::vms::api::CameraAttributesData& attributes);

    bool isRtspMetatadaRequired() const;

public slots:
    virtual void recordingEventAttached();
    virtual void recordingEventDetached();

signals:
    void vendorChanged(const QnResourcePtr& resource);
    void modelChanged(const QnResourcePtr& resource);
    void scheduleEnabledChanged(const QnResourcePtr& resource);
    void scheduleTasksChanged(const QnResourcePtr& resource);
    void groupIdChanged(const QnResourcePtr& resource, const QString& previousGroupId);
    void groupNameChanged(const QnResourcePtr& resource);
    void motionRegionChanged(const QnResourcePtr& resource);
    void motionTypeChanged(const QnResourcePtr& resource);
    void statusFlagsChanged(const QnResourcePtr& resource);
    void licenseTypeChanged(const QnResourcePtr& resource);
    void failoverPriorityChanged(const QnResourcePtr& resource);
    void backupQualityChanged(const QnResourcePtr& resource);
    void capabilitiesChanged(const QnResourcePtr& resource);
    void disableDualStreamingChanged(const QnResourcePtr& resource);
    void audioEnabledChanged(const QnResourcePtr& resource);
    void audioRequiredChanged(const QnResourcePtr& resource);
    void audioInputDeviceIdChanged(const QnResourcePtr& resource);
    void twoWayAudioEnabledChanged(const QnResourcePtr& resource);
    void audioOutputDeviceIdChanged(const QnResourcePtr& resource);
    void backupContentTypeChanged(const QnResourcePtr& resource);
    void backupPolicyChanged(const QnResourcePtr& resource);
    void mediaCapabilitiesChanged(const QnSecurityCamResourcePtr& camera);
    void cameraHotspotsEnabledChanged(const QnSecurityCamResourcePtr& camera);
    void cameraHotspotsChanged(const QnSecurityCamResourcePtr& camera);
    void recordingActionChanged(const QnResourcePtr& resource);

protected slots:
    virtual void at_motionRegionChanged();

protected:
    virtual void setSystemContext(nx::vms::common::SystemContext* systemContext) override;

    virtual void updateInternal(const QnResourcePtr& source, NotifierList& notifiers) override;

    virtual void setMotionMaskPhysical(int /*channel*/) {}

    virtual Qn::LicenseType calculateLicenseType() const;

    nx::vms::api::DeviceType enforcedDeviceType() const;

private:
    MotionTypes calculateSupportedMotionTypes() const;
    MotionType calculateMotionType() const;
    MotionStreamIndex calculateMotionStreamIndex() const;
    QPointF storedPtzPanTiltSensitivity() const;

private:
    int m_recActionCnt;
    QString m_groupName;
    QString m_groupId;
    Qn::CameraStatusFlags m_statusFlags;
    bool m_manuallyAdded;
    bool m_audioRequired = false;
    QString m_model;
    QString m_vendor;
    nx::utils::CachedValue<bool> m_cachedAudioRequired;
    nx::utils::CachedValue<bool> m_cachedRtspMetadataDisabled;
    nx::utils::CachedValue<Qn::LicenseType> m_cachedLicenseType;
    nx::utils::CachedValue<bool> m_cachedHasDualStreaming;
    nx::utils::CachedValue<MotionTypes> m_cachedSupportedMotionTypes;
    nx::utils::CachedValue<nx::vms::api::DeviceCapabilities> m_cachedCameraCapabilities;
    nx::utils::CachedValue<bool> m_cachedIsDtsBased;
    nx::utils::CachedValue<MotionType> m_motionType;
    nx::utils::CachedValue<bool> m_cachedIsIOModule;
    nx::utils::CachedValue<bool> m_cachedCanConfigureRemoteRecording;
    nx::utils::CachedValue<nx::vms::api::CameraMediaCapability> m_cachedCameraMediaCapabilities;
    nx::utils::CachedValue<nx::vms::api::DeviceType> m_cachedExplicitDeviceType;
    nx::utils::CachedValue<MotionStreamIndex> m_cachedMotionStreamIndex;
    nx::utils::CachedValue<QnIOPortDataList> m_cachedIoPorts;
    nx::utils::CachedValue<bool> m_cachedHasVideo;

protected slots:
    virtual void resetCachedValues();
};
