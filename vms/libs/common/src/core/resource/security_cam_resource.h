#pragma once

#include <mutex>
#include <map>

#include <nx/utils/thread/mutex.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/events/events_fwd.h>

#include <nx/core/resource/device_type.h>
#include <nx/utils/std/optional.h>

#include <utils/common/value_cache.h>
#include <common/common_globals.h>
#include <api/model/api_ioport_data.h>

#include <core/resource/media_resource.h>
#include <core/resource/motion_window.h>
#include <core/resource/network_resource.h>
#include <core/resource/abstract_remote_archive_manager.h>
#include <core/resource/combined_sensors_description.h>
#include <core/resource/media_stream_capability.h>
#include <core/dataprovider/live_stream_params.h>
#include <core/ptz/ptz_preset.h>
#include "resource_data.h"

class QnAbstractArchiveDelegate;

class QnSecurityCamResource : public QnNetworkResource, public QnMediaResource
{
    typedef QnNetworkResource base_type;
    Q_OBJECT

public:
    static const int kDefaultSecondStreamFpsLow;
    static const int kDefaultSecondStreamFpsMedium;
    static const int kDefaultSecondStreamFpsHigh;
    static QnUuid makeCameraIdFromUniqueId(const QString& uniqueId);

    using StreamIndex = nx::vms::api::StreamIndex;

    struct MotionStreamIndex
    {
        StreamIndex index = StreamIndex::undefined;
        bool isForced = false;
    };
public:
    QnSecurityCamResource(QnCommonModule* commonModule = nullptr);
    virtual ~QnSecurityCamResource();

    QnMediaServerResourcePtr getParentServer() const;

    Qn::MotionTypes supportedMotionType() const;
    bool isAudioSupported() const;
    bool isIOModule() const;
    Qn::MotionType getDefaultMotionType() const;
    int motionWindowCount() const;
    int motionMaskWindowCount() const;
    int motionSensWindowCount() const;
    bool hasTwoWayAudio() const;

    bool hasMotion() const;
    virtual Qn::MotionType getMotionType() const;
    void setMotionType(Qn::MotionType value);

    virtual int getMaxFps() const;

    virtual void setMaxFps(int fps);

    virtual int reservedSecondStreamFps() const;

    QList<QnMotionRegion> getMotionRegionList() const;
    void setMotionRegionList(const QList<QnMotionRegion>& maskList);

    QnMotionRegion getMotionRegion(int channel) const;
    void setMotionRegion(const QnMotionRegion& mask, int channel);

    QRegion getMotionMask(int channel) const;

    /** Returns which stream should be used for motion detection and is it forced. */
    MotionStreamIndex motionStreamIndex() const;

    /**
     * Enable forced motion detection on a selected stream or switch to automatic mode.
     */
    void setMotionStreamIndex(MotionStreamIndex value);

    /**
     * If motion detection in the remote archive is enabled. Actual for the Virtual (Wearable)
     * cameras and for the edge cameras (with RemoteArchiveCapability).
     */
    bool isRemoteArchiveMotionDetectionEnabled() const;

    /**
     * Enable or disable motion detection in the remote archive. Actual for the Virtual (Wearable)
     * cameras and for the edge cameras (with RemoteArchiveCapability).
     */
    void setRemoteArchiveMotionDetectionEnabled(bool value);

    void setScheduleTasks(const QnScheduleTaskList &scheduleTasks);
    QnScheduleTaskList getScheduleTasks() const;

    /** @return true if dual streaming is supported */
    virtual bool hasDualStreamingInternal() const;

    /** @return true if dual streaming is supported and don't blocked by user */
    virtual bool hasDualStreaming() const;

    /** Returns true if camera stores archive on a external system */
    bool isDtsBased() const;

    /** @return true if recording schedule can be configured for this device. */
    bool canConfigureRecording() const;

    /** Returns true if it is a analog camera */
    bool isAnalog() const;

    /** Returns true if it is a analog encoder (described in resource_data.json) */
    bool isAnalogEncoder() const;

    nx::vms::common::core::resource::CombinedSensorsDescription combinedSensorsDescription() const;
    void setCombinedSensorsDescription(
        const nx::vms::common::core::resource::CombinedSensorsDescription& sensorsDescription);
    bool hasCombinedSensors() const;

    /** Returns edge, analog or digital class */
    virtual Qn::LicenseType licenseType() const;

    /**
     * Returns true if all cameras in a same camera group should share 1 license
     */
    bool isSharingLicenseInGroup() const;

    bool isNvr() const;

    bool isMultiSensorCamera() const;

    nx::core::resource::DeviceType deviceType() const;

    void setDeviceType(nx::core::resource::DeviceType);

    virtual Qn::StreamFpsSharingMethod streamFpsSharingMethod() const;
    void setStreamFpsSharingMethod(Qn::StreamFpsSharingMethod value);

    // TODO: move this flags inside CameraMediaCapability struct
    Qn::CameraCapabilities getCameraCapabilities() const;
    bool hasCameraCapabilities(Qn::CameraCapabilities capabilities) const;
    void setCameraCapabilities(Qn::CameraCapabilities capabilities);
    void setCameraCapability(Qn::CameraCapability capability, bool value);

    nx::media::CameraMediaCapability cameraMediaCapability() const;
    void setCameraMediaCapability(const nx::media::CameraMediaCapability& value);

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
    bool isLicenseUsed() const;
    void setLicenseUsed(bool value);

    Qn::FailoverPriority failoverPriority() const;
    void setFailoverPriority(Qn::FailoverPriority value);

    bool isAudioEnabled() const;
    bool isAudioForced() const;
    void setAudioEnabled(bool value);

    bool isManuallyAdded() const;
    void setManuallyAdded(bool value);

    Qn::CameraBackupQualities getBackupQualities() const;
    void setBackupQualities(Qn::CameraBackupQualities value);

    /** Get backup qualities, substantiating default value. */
    Qn::CameraBackupQualities getActualBackupQualities() const;

    QString getModel() const;
    void setModel(const QString &model);

    QString getFirmware() const;
    void setFirmware(const QString &firmware);

    bool trustCameraTime() const;
    void setTrustCameraTime(bool value);

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

    void setDisableDualStreaming(bool value);
    bool isDualStreamingDisabled() const;

    void setCameraControlDisabled(bool value);
    virtual bool isCameraControlDisabled() const;

    int defaultSecondaryFps(Qn::StreamQuality quality) const;

    // TODO: #2.4 #rvasilenko #High Move to runtime data
    Qn::CameraStatusFlags statusFlags() const;
    bool hasStatusFlags(Qn::CameraStatusFlag value) const;
    void setStatusFlags(Qn::CameraStatusFlags value);
    void addStatusFlags(Qn::CameraStatusFlag value);
    void removeStatusFlags(Qn::CameraStatusFlag value);

    virtual bool needCheckIpConflicts() const;

    void setMaxDays(int value);
    int maxDays() const;

    void setMinDays(int value);
    int minDays() const;

    int recordBeforeMotionSec() const;
    void setRecordBeforeMotionSec(int value);

    int recordAfterMotionSec() const;
    void setRecordAfterMotionSec(int value);

    void setPreferredServerId(const QnUuid& value);
    QnUuid preferredServerId() const;

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

    // TODO: #rvasilenko #High #2.4 get rid of this temporary solution
    /**
     * Temporary solution to correctly detach desktop cameras when user reconnects from one server to another.
     * Implemented in QnDesktopCameraResource.
     */
    virtual bool isReadyToDetach() const {return true;}

    /**
     * @param ports Ports data to save into resource property.
     * @param needMerge If true, overrides new ports dscription with saved state, because it might
     *      be edited by client. TODO: This should be fixed by using different properties!
     * @return true Merge has happend, false otherwise.
     */
    bool setIoPortDescriptions(QnIOPortDataList ports, bool needMerge);

    /**
     * @param type Filters ports by type, does not filter if Qn::PT_Unknown.
     */
    QnIOPortDataList ioPortDescriptions(Qn::IOPortType type = Qn::PT_Unknown) const;

    virtual bool setProperty(
        const QString &key,
        const QString &value,
        PropertyOptions options = DEFAULT_OPTIONS) override;

    virtual bool setProperty(
        const QString &key,
        const QVariant& value,
        PropertyOptions options = DEFAULT_OPTIONS) override;

    virtual bool useBitratePerGop() const;

    // Allow getting multi video layout directly from a RTSP SDP info
    virtual bool allowRtspVideoLayout() const { return true; }

    /**
     * Return non zero media event error if camera resource has an issue.
     */
    Qn::MediaStreamEvent checkForErrors() const;

    bool isEnoughFpsToRunSecondStream(int currentFps) const;
    virtual nx::core::resource::AbstractRemoteArchiveManager* remoteArchiveManager();

    virtual void analyticsEventStarted(const QString& caption, const QString& description);
    virtual void analyticsEventEnded(const QString& caption, const QString& description);

    virtual int suggestBitrateKbps(
        const QnLiveStreamParams& streamParams, Qn::ConnectionRole role) const;
    static float rawSuggestBitrateKbps(
        Qn::StreamQuality quality, QSize resolution, int fps, const QString& codec);

    /**
     * All events emitted by analytics driver bound to the resource can be captured within
     * this method.
     * @return true if event has been captured, false otherwise.
     */
    virtual bool captureEvent(const nx::vms::event::AbstractEventPtr& event);
    virtual bool isAnalyticsDriverEvent(nx::vms::api::EventType eventType) const;

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

    bool isUserAllowedToModifyPtzCapabilities() const;

    void setIsUserAllowedToModifyPtzCapabilities(bool allowed);

    Ptz::Capabilities ptzCapabilitiesAddedByUser() const;

    void setPtzCapabilitiesAddedByUser(Ptz::Capabilities capabilities);

    QnResourceData resourceData() const;

    virtual void setCommonModule(QnCommonModule* commonModule) override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() = 0;
public slots:
    virtual void recordingEventAttached();
    virtual void recordingEventDetached();

signals:
    void licenseUsedChanged(const QnResourcePtr &resource);
    void scheduleTasksChanged(const QnResourcePtr &resource);
    void groupIdChanged(const QnResourcePtr &resource);
    void groupNameChanged(const QnResourcePtr &resource);
    void motionRegionChanged(const QnResourcePtr &resource);
    void statusFlagsChanged(const QnResourcePtr &resource);
    void licenseTypeChanged(const QnResourcePtr &resource);
    void failoverPriorityChanged(const QnResourcePtr &resource);
    void backupQualitiesChanged(const QnResourcePtr &resource);
    void capabilitiesChanged(const QnResourcePtr& resource);
    void disableDualStreamingChanged(const QnResourcePtr& resource);
    void audioEnabledChanged(const QnResourcePtr &resource);
    void ptzConfigurationChanged(const QnResourcePtr &resource);

    void networkIssue(const QnResourcePtr&, qint64 timeStamp, nx::vms::api::EventReason reasonCode, const QString& reasonParamsEncoded);

    void analyticsEventStart(
        const QnResourcePtr& resource,
        const QString& caption,
        const QString& description,
        qint64 timestamp);

    void analyticsEventEnd(
        const QnResourcePtr& resource,
        const QString& caption,
        const QString& description,
        qint64 timestamp );
protected slots:
    virtual void at_initializedChanged();
    virtual void at_motionRegionChanged();

protected:
    virtual void updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers) override;

    virtual void initializationDone() override;

    virtual void setMotionMaskPhysical(int /*channel*/) {}

    virtual Qn::LicenseType calculateLicenseType() const;

private:
    Qn::MotionTypes calculateSupportedMotionType() const;
    Qn::MotionType calculateMotionType() const;
    MotionStreamIndex calculateMotionStreamIndex() const;

private:
    int m_recActionCnt;
    QString m_groupName;
    QString m_groupId;
    Qn::CameraStatusFlags m_statusFlags;
    bool m_manuallyAdded;
    QString m_model;
    QString m_vendor;
    CachedValue<Qn::LicenseType> m_cachedLicenseType;
    CachedValue<bool> m_cachedHasDualStreaming;
    CachedValue<Qn::MotionTypes> m_cachedSupportedMotionType;
    CachedValue<Qn::CameraCapabilities> m_cachedCameraCapabilities;
    CachedValue<bool> m_cachedIsDtsBased;
    CachedValue<Qn::MotionType> m_motionType;
    CachedValue<bool> m_cachedIsIOModule;
    CachedValue<bool> m_cachedCanConfigureRemoteRecording;
    CachedValue<nx::media::CameraMediaCapability> m_cachedCameraMediaCapabilities;
    CachedValue<nx::core::resource::DeviceType> m_cachedDeviceType;
    CachedValue<bool> m_cachedHasVideo;
    CachedValue<MotionStreamIndex> m_cachedMotionStreamIndex;

private slots:
    void resetCachedValues();
};

Q_DECLARE_METATYPE(QnSecurityCamResourcePtr)
Q_DECLARE_METATYPE(QnSecurityCamResourceList)
