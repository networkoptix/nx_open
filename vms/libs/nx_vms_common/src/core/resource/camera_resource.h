// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <set>

#include <QtCore/QElapsedTimer>

#include <api/model/api_ioport_data.h>
#include <common/common_globals.h>
#include <core/dataprovider/live_stream_params.h>
#include <core/misc/schedule_task.h>
#include <core/ptz/ptz_constants.h>
#include <core/resource/camera_media_stream_info.h>
#include <core/resource/media_resource.h>
#include <core/resource/resource_fwd.h>
#include <nx/core/resource/using_media2_type.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/socket_common.h>
#include <nx/utils/lockable.h>
#include <nx/utils/mac_address.h>
#include <nx/utils/safe_direct_connection.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>
#include <nx/utils/value_cache.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/vms/api/data/device_model.h>
#include <nx/vms/api/data/device_profile.h>
#include <nx/vms/api/data/media_stream_capability.h>
#include <nx/vms/api/types/rtp_types.h>
#include <nx/vms/common/ptz/type.h>
#include <nx/vms/common/resource/camera_hotspots_data.h>
#include <nx_ec/ec_api_fwd.h>
#include <recording/time_period_list.h>
#include <utils/common/aspect_ratio.h>

class QAuthenticator;
class QnAbstractArchiveDelegate;
class QnResourceData;
class QnMotionRegion;

namespace nx::core::resource { class AbstractRemoteArchiveManager; }
namespace nx::media { enum class StreamEvent; }

class NX_VMS_COMMON_API QnVirtualCameraResource:
    public QnMediaResource,
    public /*mixin*/ Qn::EnableSafeDirectConnection
{
    Q_OBJECT
    Q_PROPERTY(nx::vms::api::DeviceCapabilities cameraCapabilities
        READ getCameraCapabilities WRITE setCameraCapabilities)
    Q_PROPERTY(Ptz::Capabilities ptzCapabilities
        READ getPtzCapabilities WRITE setPtzCapabilities NOTIFY ptzCapabilitiesChanged)
    using base_type = QnMediaResource;

public:
    static constexpr int kDefaultMaxFps = 15;
    static nx::Uuid makeCameraIdFromPhysicalId(const QString& physicalId);
    static QString intercomSpecificPortName();

    static const QString kCompatibleAnalyticsEnginesProperty;
    static const QString kUserEnabledAnalyticsEnginesProperty;
    // This property kept here only because of statistics filtering.
    static const QString kAnalyzedStreamIndexes;
    static const QString kVirtualCameraIgnoreTimeZone;
    static const QString kHttpPortParameterName;
    static const QString kCameraNameParameterName;
    static const QString kUsingOnvifMedia2Type;

    struct NX_VMS_COMMON_API MotionStreamIndex
    {
        nx::vms::api::StreamIndex index = nx::vms::api::StreamIndex::undefined;
        bool isForced = false;

        bool operator==(const MotionStreamIndex&) const = default;
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
    QnVirtualCameraResource();
    virtual ~QnVirtualCameraResource() override;

    QnMediaServerResourcePtr getParentServer() const;

    /**
     * Identifier of the Device type. Used to access Device read-only or default properties (e.g.
     * the maximum fps value or sensor configuration). Such parameters are stored in a separate
     * json file, available online. The Server regularly reads it, updates the internal values and
     * provides them to the connecting Clients.
     */
    virtual nx::Uuid getTypeId() const override;

    /**
     * Updates the Device type id. Actual when the same Device is found by a
     * new more actual driver, or when a new Resource type is added to the Device Type Pool.
     */
    virtual void setTypeId(const nx::Uuid& id) override;

    /**
     * Helper function to check if the Device status is "online" or "recording". Must not be used
     * anywhere else.
     */
    virtual bool isOnline() const override { return isOnline(getStatus()); }

    /**
     * Helper function to check if the Device status is "online" or "recording". Must not be used
     * anywhere else.
     */
    static bool isOnline(nx::vms::api::ResourceStatus status)
    {
        return status == nx::vms::api::ResourceStatus::online
            || status == nx::vms::api::ResourceStatus::recording;
    }

    virtual QString getProperty(const QString& key) const override;

    // TODO: #skolesnik nx::Url
    virtual void setUrl(const QString& url) override;

    virtual nx::network::HostAddress getHostAddress() const;
    virtual void setHostAddress(const nx::network::HostAddress &ip);

    // TODO: #skolesnik Remove unsafe QString
    virtual void setHostAddress(const QString &ip);

    nx::utils::MacAddress getMAC() const;
    void setMAC(const nx::utils::MacAddress &mac);

    virtual QString getPhysicalId() const;
    void setPhysicalId(const QString& physicalId);

    void setAuth(const QString &user, const QString &password);
    void setAuth(const QAuthenticator &auth);

    void setDefaultAuth(const QString &user, const QString &password);
    void setDefaultAuth(const QAuthenticator &auth);

    /**
     * Parse user and password from the colon-delimited string.
     */
    static QAuthenticator parseAuth(const QString& value);

    QAuthenticator getAuth() const;
    QAuthenticator getDefaultAuth() const;

    virtual std::uint16_t httpPort() const;
    virtual void setHttpPort(std::uint16_t newPort);

    /* By default, it is rtsp port (554). */
    virtual int mediaPort() const;
    void setMediaPort(int value);

    // This value is updated by discovery process.
    QDateTime getLastDiscoveredTime() const;
    void setLastDiscoveredTime(const QDateTime &time);

    // All data readers and any sockets will use this number as timeout value.
    virtual std::chrono::milliseconds getNetworkTimeout() const;

    // In some cases it is necessary to update only a couple of field from just discovered resource.
    virtual bool mergeResourcesIfNeeded(const QnVirtualCameraResourcePtr& source);

    virtual int getChannel() const;

    /**
     * Returns true if camera is accessible
     * Default implementation just establishes connection to \a getHostAddress() : \a httpPort()
     * TODO: #akolesnikov This method is used in diagnostics only. Throw it away and use \a
     *     QnVirtualCameraResource::checkIfOnlineAsync instead.
     */
    virtual bool ping();

    /**
     * Checks if camera is online
     * \param completionHandler Invoked on check completion. Check result is passed to the functor
     * \return true if async operation has been started. false otherwise
     * \note Implementation MUST check not only camera address:port accessibility, but also check
     * some unique parameters of camera
     * \note Default implementation returns false
     */
    virtual void checkIfOnlineAsync(std::function<void(bool)> completionHandler);

    static nx::Uuid physicalIdToId(const QString& uniqId);

    virtual QString idForToStringFromPtr() const override;

    static QString mediaPortKey();
    bool isAudioSupported() const;
    bool isIOModule() const;
    int motionWindowCount() const;
    int motionMaskWindowCount() const;
    int motionSensWindowCount() const;
    bool hasTwoWayAudio() const;

    Ptz::Capabilities getPtzCapabilities(
        nx::vms::common::ptz::Type ptzType = nx::vms::common::ptz::Type::operational) const;

    /** Check if device has any of provided capabilities. */
    bool hasAnyOfPtzCapabilities(
        Ptz::Capabilities capabilities,
        nx::vms::common::ptz::Type ptzType = nx::vms::common::ptz::Type::operational) const;
    void setPtzCapabilities(
        Ptz::Capabilities capabilities,
        nx::vms::common::ptz::Type ptzType = nx::vms::common::ptz::Type::operational);
    void setPtzCapability(
        Ptz::Capabilities capability,
        bool value,
        nx::vms::common::ptz::Type ptzType = nx::vms::common::ptz::Type::operational);

    bool canSwitchPtzPresetTypes() const;

    /**
     * Actual motion type used when the `MotionType::default_` value is selected.
     */
    nx::vms::api::MotionType getDefaultMotionType() const;

    /**
     * Flags set for all motion types, supported by the Camera driver.
     */
    nx::vms::api::MotionTypes supportedMotionTypes() const;

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
    virtual nx::vms::api::MotionType getMotionType() const;

    /**
     * Select preferred motion detection type. `MotionType::MT_None` should be passed to disable
     * motion detection.
     */
    void setMotionType(nx::vms::api::MotionType value);

    virtual int getMaxFps(nx::vms::api::StreamIndex streamIndex = nx::vms::api::StreamIndex::primary) const;

    virtual void setMaxFps(int fps, nx::vms::api::StreamIndex streamIndex = nx::vms::api::StreamIndex::primary);

    virtual int reservedSecondStreamFps() const;

    QList<QnMotionRegion> getMotionRegionList() const;
    void setMotionRegionList(const QList<QnMotionRegion>& maskList);

    QnMotionRegion getMotionRegion() const;

    /** Returns which stream should be used for motion detection and whether it is forced. */
    virtual MotionStreamIndex motionStreamIndex() const;
    MotionStreamIndex motionStreamIndexInternal() const;

    /** Enable forced motion detection on a selected stream or switch to automatic mode. */
    void setMotionStreamIndex(MotionStreamIndex value);

    /**
     * If motion detection in the remote archive is enabled. Actual for the virtual
     * devices and for the edge devices (with RemoteArchiveCapability).
     */
    bool isRemoteArchiveMotionDetectionEnabled() const;

    /**
     * Enable or disable motion detection in the remote archive. Actual for the virtual
     * devices and for the edge devices (with RemoteArchiveCapability).
     */
    void setRemoteArchiveMotionDetectionEnabled(bool value);

    void setScheduleTasks(const QnScheduleTaskList &scheduleTasks);
    QnScheduleTaskList getScheduleTasks() const;

    /** @return Whether dual streaming is supported by the device internally. */
    virtual bool hasDualStreamingInternal() const;

    /**
     * @return Whether dual streaming is supported by the device and is not forcefully disabled by
     * a user (@see `isDualStreamingDisabled()`).
     */
    virtual bool hasDualStreaming() const;

    /** Returns true if device stores archive on a external system */
    bool isDtsBased() const;

    /** @return True if recording schedule can be configured for this device. */
    bool supportsSchedule() const;

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
    void updateCameraMediaCapability(
        std::function<void(nx::vms::api::CameraMediaCapability& capability)> updater);

    virtual QnAbstractArchiveDelegate* createArchiveDelegate() { return 0; }

    // Returns user-defined camera name (if not empty), default name otherwise
    QString getUserDefinedName() const;

    // Returns user-defined group name (if not empty) or default group name
    virtual QString getUserDefinedGroupName() const;
    // Returns default group name
    QString getDefaultGroupName() const;
    virtual void setDefaultGroupName(const QString& value);

    /**
     * Set group name (the one is show to the user in client)
     * This name is set by user.
     * \a setDefaultGroupName name is generally set automatically (e.g., by server)
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

    /** @returns True if the audio should not be disabled */
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

    /** @return ID of device which used as audio input override for the camera. */
    nx::Uuid audioInputDeviceId() const;

    /** @return Redirected audio input (if any) or self. */
    QnVirtualCameraResourcePtr audioInputDevice() const;

    /**
     * Sets ID of device which will be used as audio input override for the camera.
     * @param deviceId Valid another device ID expected. Null ID should be set if this device's
     *     own audio input is intended to be used.
     */
    void setAudioInputDeviceId(const nx::Uuid& deviceId);

    /**
     * @returns True if the audio transmitting is enabled. Enabled state doesn't guarantee
     *     actual transmission possibility since audio output may be not configured right.
     */
    bool isTwoWayAudioEnabled() const;

    /** Sets whether audio transmitting should be enabled or disabled. */
    void setTwoWayAudioEnabled(bool value);

    /** @return ID of device which used as audio output override for the camera. */
    nx::Uuid audioOutputDeviceId() const;

    /** @return Redirected audio output (if any) or self. */
    QnVirtualCameraResourcePtr audioOutputDevice() const;

    /**
     * Sets ID of device which will be used as audio output override for the camera.
     * @param deviceId Valid another device ID expected. Null ID should be set if this device's
     *     own audio output is intended to be used.
     */
    void setAudioOutputDeviceId(const nx::Uuid& deviceId);

    bool isManuallyAdded() const;
    void setManuallyAdded(bool value);

    nx::vms::api::CameraBackupQuality getBackupQuality() const;
    void setBackupQuality(nx::vms::api::CameraBackupQuality value);

    /** Get backup qualities, substantiating default value. */
    nx::vms::api::CameraBackupQuality getActualBackupQualities() const;

    /** @return Whether camera hotspots enabled or not. */
    bool cameraHotspotsEnabled() const;

    /** Sets whether camera hotspots enabled or not. */
    void setCameraHotspotsEnabled(bool enabled);

    /** @return List of camera hotspots description data structures. */
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

    QString getSerialNumber() const;
    void setSerialNumber(const QString& serialNumber);

    bool trustCameraTime() const;
    void setTrustCameraTime(bool value);

    bool keepCameraTimeSettings() const;
    void setKeepCameraTimeSettings(bool value);
    bool defaultKeepCameraTimeSettingsState() const;

    QString getVendor() const;
    void setVendor(const QString &value);

    // TODO: #skolesnik There are just a couple of places where `logicalId()` is used from
    // `QnResource` (including Desktop client's code)
    /**
     * User-defined id which primary usage purpose is to support different integrations. By using
     * this id, the user can easily switch one Device with another, and all API requests continue
     * to work as usual.
     *
     * Supported for Devices and Layouts only. Ids should be unique between the same types (not
     * enforced though), but the same logical id for a Device and a Layout is perfectly OK as they
     * are used in different API calls.
     */
    virtual int logicalId() const override;
    virtual void setLogicalId(int value) override;

    bool isGroupPlayOnly() const;

    bool needsToChangeDefaultPassword() const;

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

    void setPreferredServerId(const nx::Uuid& value);
    nx::Uuid preferredServerId() const;

    bool isRemoteArchiveSynchronizationEnabled() const;
    void setRemoteArchiveSynchronizationEnabled(bool value);
    bool defaultRemoteArchiveSynchronizationEnabledState() const;

    /** If preferred server is not set, assigns current server as preferred. */
    void updatePreferredServerId();

    /**
     * Returns list of time periods of DTS archive, containing motion at specified \a regions with
     * timestamp in region [\a msStartTime; \a msEndTime)
     * \param detailLevel Minimal time period gap (usec) that is of interest to the caller.
     * Two time periods lying closer to each other than \a detailLevel usec SHOULD be reported as
     * one
     * \note Used only if \a QnVirtualCameraResource::isDtsBased() is \a true
     * \note Default implementation does nothing
     */
    virtual QnTimePeriodList getDtsTimePeriodsByMotionRegion(
        const QList<QRegion>& regions,
        std::chrono::milliseconds msStartTime,
        std::chrono::milliseconds msEndTime,
        int detailLevel,
        bool keepSmalChunks,
        int limit,
        Qt::SortOrder sortOrder);

    /**
     * @param ports Ports data to save into resource property.
     * @param needMerge If true, overrides new ports description with saved state, because it might
     *      be edited by client. TODO: This should be fixed by using different properties!
     * @return true Merge has happened, false otherwise.
     */
    virtual bool setIoPortDescriptions(QnIOPortDataList ports, bool needMerge);

    /** @param type Filters ports by type, does not filter if Qn::PT_Unknown. */
    QnIOPortDataList ioPortDescriptions(Qn::IOPortType type = Qn::PT_Unknown) const;

    bool isIntercom() const;

    nx::vms::api::ExtendedCameraOutputs extendedOutputs() const;

    virtual bool useBitratePerGop() const;

    virtual nx::core::resource::UsingOnvifMedia2Type useMedia2ToFetchProfiles() const;

    // Allow getting multi video layout directly from a RTSP SDP info
    virtual bool allowRtspVideoLayout() const { return true; }

    /** Return non zero media event error if camera resource has an issue. */
    nx::media::StreamEvent checkForErrors() const;

    virtual nx::core::resource::AbstractRemoteArchiveManager* remoteArchiveManager();

    virtual float suggestBitrateKbps(
        const QnLiveStreamParams& streamParams, Qn::ConnectionRole role) const;
    static float rawSuggestBitrateKbps(
        Qn::StreamQuality quality, QSize resolution, int fps, const QString& codec);

    virtual bool hasVideo(const QnAbstractStreamDataProvider* dataProvider = nullptr) const override;

    /** Whether the camera has audio, concerning redirected audio input device if it is set. */
    virtual bool hasAudio() const;

    /**
     * Update user password at the camera. This function is able to change password for existing user only.
     */
    virtual bool setCameraCredentialsSync(
        const QAuthenticator& auth, QString* outErrorString = nullptr);

    /** Returns true if camera credential was auto detected by media server. */
    bool isDefaultAuth() const;

    virtual int suggestBitrateForQualityKbps(Qn::StreamQuality q, QSize resolution, int fps,
        const QString& codec, Qn::ConnectionRole role = Qn::CR_Default) const;

    static Qn::ConnectionRole toConnectionRole(nx::vms::api::StreamIndex index);
    static nx::vms::api::StreamIndex toStreamIndex(Qn::ConnectionRole role);

    nx::vms::api::ptz::PresetType preferredPtzPresetType() const;

    nx::vms::api::ptz::PresetType userPreferredPtzPresetType() const;
    void setUserPreferredPtzPresetType(nx::vms::api::ptz::PresetType);

    nx::vms::api::ptz::PresetType defaultPreferredPtzPresetType() const;
    void setDefaultPreferredPtzPresetType(nx::vms::api::ptz::PresetType);

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

    // TODO: #skolesnik nx::Url ?
    virtual QString getUrl() const override;

    nx::vms::api::CameraAttributesData getUserAttributes() const;
    void setUserAttributes(const nx::vms::api::CameraAttributesData& attributes);
    void setUserAttributesAndNotify(const nx::vms::api::CameraAttributesData& attributes);

    bool isRtspMetatadaRequired() const;
    void forceEnableAudio();
    void forceDisableAudio();
    bool isForcedAudioSupported() const;

    void updateDefaultAuthIfEmpty(const QString& login, const QString& password);

    //! Camera source URL, commonly - rtsp link.
    QString sourceUrl(Qn::ConnectionRole role) const;
    void updateSourceUrl(const nx::Url& url, Qn::ConnectionRole role, bool save = true);

    bool virtualCameraIgnoreTimeZone() const;
    /**
     * Meaningful only for virtual camera.
     * If the value is true, video's timestamps is considered as local client time.
     * Otherwise, timestamps is considered as UTC.
     */
    void setVirtualCameraIgnoreTimeZone(bool value);

    nx::vms::api::RtpTransportType preferredRtpTransport() const;
    CameraMediaStreams mediaStreams() const;
    CameraMediaStreamInfo streamInfo(nx::vms::api::StreamIndex index = nx::vms::api::StreamIndex::primary) const;

    /** @return frame aspect ratio of a single channel. Does not account for default rotation. */
    virtual QnAspectRatio aspectRatio() const;

    /** @return frame aspect ratio of a single channel. Accounts for default rotation. */
    virtual QnAspectRatio aspectRatioRotated() const;

    /**
     * @return Ids of Analytics Engines which are actually compatible with the Device, enabled by
     *     the user and active (running on the current server). Includes device-dependent Engines.
     */
    virtual std::set<nx::Uuid> enabledAnalyticsEngines() const;

    /**
     * @return Analytics Engines which are actually compatible with the Device, enabled by the user
     * and active (running on the current server). Includes device-dependent Engines.
     */
    const nx::vms::common::AnalyticsEngineResourceList enabledAnalyticsEngineResources() const;

    /**
     * @return Ids of Analytics Engines which are explicitly enabled by the user. Not validated for
     *     compatibility with the Device or if the engine is active (running on the current server).
     */
    std::set<nx::Uuid> userEnabledAnalyticsEngines() const;

    /**
     * Set ids of Analytics Engines which are explicitly enabled by the user. Not validated for
     * compatibility with the Device or if the Engine is active (running on the current server).
     */
    void setUserEnabledAnalyticsEngines(const std::set<nx::Uuid>& engines);

    /**
     * Set ids of Analytics Engines which are explicitly enabled by the user.
     * This is the same function like 'setUserEnabledAnalyticsEngines' but it returns serialized data instead of storing
     * it to the resource properties.
     */
    nx::vms::api::ResourceParamData serializeUserEnabledAnalyticsEngines(
        const std::set<nx::Uuid>& engines);

    /**
     * @return Ids of Analytics Engines which can be potentially used with the Device. Only active
     *     (running on the current server) Engines are included.
     */
    virtual const std::set<nx::Uuid> compatibleAnalyticsEngines() const;

    /**
     * @return Analytics Engines which can be potentially used with the Device. Only active
     *     (running on the current server) Engines are included.
     */
    virtual nx::vms::common::AnalyticsEngineResourceList compatibleAnalyticsEngineResources() const;

    /**
     * Set ids of Analytics Engines which can be potentially used with the Device. Only active
     * (running on the current server) Engines must be included.
     */
    void setCompatibleAnalyticsEngines(const std::set<nx::Uuid>& engines);

    using AnalyticsEntitiesByEngine = std::map<nx::Uuid, std::set<QString>>;

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
        const nx::Uuid& engineId) const;

    void setDeviceAgentManifest(
        const nx::Uuid& engineId,
        const nx::vms::api::analytics::DeviceAgentManifest& manifest);

    nx::vms::api::StreamIndex analyzedStreamIndex(nx::Uuid engineId) const;

    nx::vms::api::StreamIndex defaultAnalyzedStreamIndex() const;

    void setAnalyzedStreamIndex(nx::Uuid engineId, nx::vms::api::StreamIndex streamIndex);

    /** Whether this camera supports web page. */
    bool isWebPageSupported() const;

    /**
     * Customized user-provided port for Camera Web Page.
     * @return positive value if custom port is set; 0 if default port should be used.
     */
    int customWebPagePort() const;

    /** Set custom port for Camera Web Page. Pass 0 to reset custom value. */
    void setCustomWebPagePort(int value);

    /**
     * Checks if motion detection is enabled and actually can work (hardware, or not exceeding
     * resolution limit or is forced).
     */
    bool isMotionDetectionActive() const;

    /** Checks if schedule task is applicable: recording mode and metadata types are supported. */
    static bool canApplyScheduleTask(
        const QnScheduleTask& task,
        bool dualStreamingAllowed,
        bool motionDetectionAllowed,
        bool objectDetectionAllowed);

    /** Checks if schedule task is applicable: recording mode and metadata types are supported. */
    bool canApplyScheduleTask(const QnScheduleTask& task) const;

    /** Checks if all schedule tasks are applicable. */
    static bool canApplySchedule(
        const QnScheduleTaskList& schedule,
        bool dualStreamingAllowed,
        bool motionDetectionAllowed,
        bool objectDetectionAllowed);

    /** Checks if all schedule tasks are applicable. */
    bool canApplySchedule(const QnScheduleTaskList& schedule) const;

    static constexpr QSize kMaximumSecondaryStreamResolution{1024, 768};
    static constexpr int kMaximumMotionDetectionPixels
        = kMaximumSecondaryStreamResolution.width() * kMaximumSecondaryStreamResolution.height();

    /**
     * Set a list of available device profiles. It can be used on camera Expert dialog
     * to manually setup camera profile to use
     */
    void setAvailableProfiles(const nx::vms::api::DeviceProfiles& value);

    /** Get a list of available device profiles. */
    nx::vms::api::DeviceProfiles availableProfiles() const;

    /** Manually setup a profile ID to use. */
    void setForcedProfile(const QString& id, nx::vms::api::StreamIndex index);

    /** Read manually configured profile ID to use. Can be empty. */
    QString forcedProfile(nx::vms::api::StreamIndex index) const;

    using DeviceAgentManifestMap = std::map<nx::Uuid, nx::vms::api::analytics::DeviceAgentManifest>;
    DeviceAgentManifestMap deviceAgentManifests() const;

    int backupMegapixels() const;
    int backupMegapixels(nx::vms::api::CameraBackupQuality quality) const;

    QnLiveStreamParams streamConfiguration(nx::vms::api::StreamIndex stream) const;
    nx::Url vmsCloudUrl() const;
    bool isVmsCloudUrl() const;

    QSize maxVideoResolution() const;

signals:
    void ptzCapabilitiesChanged(const QnVirtualCameraResourcePtr& camera);
    void userEnabledAnalyticsEnginesChanged(const QnVirtualCameraResourcePtr& camera);
    void compatibleAnalyticsEnginesChanged(const QnVirtualCameraResourcePtr& camera);
    void deviceAgentManifestsChanged(const QnVirtualCameraResourcePtr& camera);
    void isIOModuleChanged(const QnVirtualCameraResourcePtr& camera);
    void hasVideoChanged(const QnVirtualCameraResourcePtr& camera);

    // TODO: Get rid of this "maybe" logic.
    void compatibleEventTypesMaybeChanged(const QnVirtualCameraResourcePtr& camera);
    void compatibleObjectTypesMaybeChanged(const QnVirtualCameraResourcePtr& camera);

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
    void mediaCapabilitiesChanged(const QnVirtualCameraResourcePtr& camera);
    void cameraHotspotsEnabledChanged(const QnVirtualCameraResourcePtr& camera);
    void cameraHotspotsChanged(const QnVirtualCameraResourcePtr& camera);
    void ioPortDescriptionsChanged(const QnVirtualCameraResourcePtr& camera);

protected:
    /** Returns true if it is a analog device */
    bool isAnalog() const;

protected slots:
    virtual void at_motionRegionChanged();
    virtual void resetCachedValues();

protected:
    virtual void emitPropertyChanged(
        const QString& key, const QString& prevValue, const QString& newValue) override;

    virtual void setSystemContext(nx::vms::common::SystemContext* systemContext) override;

    virtual void updateInternal(const QnResourcePtr& source, NotifierList& notifiers) override;

    virtual void setMotionMaskPhysical(int /*channel*/) {}

    virtual Qn::LicenseType calculateLicenseType() const;

    nx::vms::api::DeviceType enforcedDeviceType() const;

private:
    nx::utils::MacAddress m_macAddress;
    QString m_physicalId;

    // Initialized in cpp to avoid transitional includes.
    std::uint16_t m_httpPort;

    QDateTime m_lastDiscoveredTime;

    nx::utils::CachedValue<nx::network::HostAddress> m_cachedHostAddress;

    /** Identifier of the type of this Device. */
    nx::Uuid m_typeId;

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
    nx::utils::CachedValue<nx::vms::api::MotionTypes> m_cachedSupportedMotionTypes;
    nx::utils::CachedValue<nx::vms::api::DeviceCapabilities> m_cachedCameraCapabilities;
    nx::utils::CachedValue<bool> m_cachedIsDtsBased;
    nx::utils::CachedValue<nx::vms::api::MotionType> m_motionType;
    nx::utils::CachedValue<bool> m_cachedIsIOModule;
    nx::utils::CachedValue<bool> m_cachedCanConfigureRemoteRecording;
    nx::utils::CachedValue<nx::vms::api::CameraMediaCapability> m_cachedCameraMediaCapabilities;
    nx::utils::CachedValue<nx::vms::api::DeviceType> m_cachedExplicitDeviceType;
    nx::utils::CachedValue<MotionStreamIndex> m_cachedMotionStreamIndex;
    nx::utils::CachedValue<QnIOPortDataList> m_cachedIoPorts;
    nx::utils::CachedValue<bool> m_cachedHasVideo;
    nx::utils::CachedValue<bool> m_isIntercom;

    nx::utils::CachedValue<std::set<nx::Uuid>> m_cachedUserEnabledAnalyticsEngines;
    nx::utils::CachedValue<std::set<nx::Uuid>> m_cachedCompatibleAnalyticsEngines;
    nx::utils::CachedValue<DeviceAgentManifestMap> m_cachedDeviceAgentManifests;
    nx::utils::CachedValue<std::map<nx::Uuid, std::set<QString>>> m_cachedSupportedEventTypes;
    nx::utils::CachedValue<std::map<nx::Uuid, std::set<QString>>> m_cachedSupportedObjectTypes;
    mutable nx::Lockable<std::map<nx::Uuid, nx::vms::api::StreamIndex>> m_cachedAnalyzedStreamIndex;
    nx::utils::CachedValue<CameraMediaStreams> m_cachedMediaStreams;
    nx::utils::CachedValue<int> m_cachedBackupMegapixels;
    nx::utils::CachedValue<QnLiveStreamParams> m_primaryStreamConfiguration;
    nx::utils::CachedValue<QnLiveStreamParams> m_secondaryStreamConfiguration;

    mutable nx::Mutex m_cachedValueMutex;
};

constexpr QSize EMPTY_RESOLUTION_PAIR(0, 0);
constexpr QSize SECONDARY_STREAM_MAX_RESOLUTION =
    QnVirtualCameraResource::kMaximumSecondaryStreamResolution;
constexpr QSize UNLIMITED_RESOLUTION(INT_MAX, INT_MAX);
