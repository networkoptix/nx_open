#ifndef sequrity_cam_resource_h_1239
#define sequrity_cam_resource_h_1239

#include <mutex>
#include <map>

#include <nx/utils/thread/mutex.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/api/analytics/supported_events.h>
#include <nx/vms/event/events/events_fwd.h>

#include <utils/common/value_cache.h>
#include <common/common_globals.h>
#include <api/model/api_ioport_data.h>

#include <core/dataconsumer/audio_data_transmitter.h>
#include <core/misc/schedule_task.h>

#include <core/resource/media_resource.h>
#include <core/resource/motion_window.h>
#include <core/resource/network_resource.h>
#include <core/resource/abstract_remote_archive_manager.h>
#include <core/resource/combined_sensors_description.h>
#include <core/resource/media_stream_capability.h>

class QnAbstractArchiveDelegate;
class QnDataProviderFactory;

#ifdef ENABLE_DATA_PROVIDERS
typedef std::shared_ptr<QnAbstractAudioTransmitter> QnAudioTransmitterPtr;
#endif

static const int PRIMARY_ENCODER_INDEX = 0;
static const int SECONDARY_ENCODER_INDEX = 1;

class QnSecurityCamResource : public QnNetworkResource, public QnMediaResource
{
    typedef QnNetworkResource base_type;
    Q_OBJECT

public:
    static QnUuid makeCameraIdFromUniqueId(const QString& uniqueId);

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
    Qn::MotionType getMotionType() const;
    void setMotionType(Qn::MotionType value);

    //!Returns driver (built-in or external) name, used to manage camera
    /*!
        This can be "axis", "dlink", "onvif", etc.
    */
    virtual QString getDriverName() const = 0;

    virtual int getMaxFps() const;

    virtual int reservedSecondStreamFps() const;

    /** sets the distance between I frames */
    virtual void setIframeDistance(int /*frames*/, int /*timems*/) {}

    void setDataProviderFactory(QnDataProviderFactory* dpFactory);

    QList<QnMotionRegion> getMotionRegionList() const;
    void setMotionRegionList(const QList<QnMotionRegion>& maskList);

    QnMotionRegion getMotionRegion(int channel) const;
    void setMotionRegion(const QnMotionRegion& mask, int channel);

    QRegion getMotionMask(int channel) const;

    ////!Get camera settings, which are generally modified by user
    ///*!
    //    E.g., recording schedule, motion type, second stream quality, etc...
    //*/
    //QnCameraUserAttributes getUserCameraSettings() const;

    void setScheduleTasks(const QnScheduleTaskList &scheduleTasks);
    QnScheduleTaskList getScheduleTasks() const;

    virtual bool hasDualStreaming() const;

    /** Return true if dual streaming supported and don't blocked by user */
    bool hasDualStreaming2() const;

    /** Returns true if camera stores archive on a external system */
    bool isDtsBased() const;

    /** Returns true if it is a analog camera */
    bool isAnalog() const;

    /** Returns true if it is a analog encoder (described in resource_data.json) */
    bool isAnalogEncoder() const;

    nx::vms::common::core::resource::CombinedSensorsDescription combinedSensorsDescription() const;
    void setCombinedSensorsDescription(
        const nx::vms::common::core::resource::CombinedSensorsDescription& sensorsDescription);
    bool hasCombinedSensors() const;

    /** Returns true if it is a edge camera */
    bool isEdge() const;

    /** Returns edge, analog or digital class */
    Qn::LicenseType licenseType() const;

    /**
     * Returns true if all cameras in a same camera group should share 1 license
     */
    bool isSharingLicenseInGroup() const;


    virtual Qn::StreamFpsSharingMethod streamFpsSharingMethod() const;
    void setStreamFpsSharingMethod(Qn::StreamFpsSharingMethod value);

    virtual QnIOPortDataList getRelayOutputList() const;
    virtual QnIOPortDataList getInputPortList() const;

    // TODO: move this flags inside CameraMediaCapability struct
    Qn::CameraCapabilities getCameraCapabilities() const;
    bool hasCameraCapabilities(Qn::CameraCapabilities capabilities) const;
    void setCameraCapabilities(Qn::CameraCapabilities capabilities);
    void setCameraCapability(Qn::CameraCapability capability, bool value);

    nx::media::CameraMediaCapability cameraMediaCapability() const;

    /*!
        Change output with id \a ouputID state to \a activate
        \param ouputID If empty, implementation MUST select any output port
        \param autoResetTimeoutMS If > 0 and \a activate is \a true, than output will be deactivated in \a autoResetTimeout milliseconds
        \return true in case of success. false, if nothing has been done
    */
    virtual bool setRelayOutputState(const QString& ouputID, bool activate, unsigned int autoResetTimeoutMS = 0);

    bool isRecordingEventAttached() const;

    virtual QnAbstractArchiveDelegate* createArchiveDelegate() { return 0; }
    virtual QnAbstractStreamDataProvider* createArchiveDataProvider() { return 0; }

    //!Returns user-defined camera name (if not empty), default name otherwise
    QString getUserDefinedName() const;

    //!Returns user-defined group name (if not empty) or server-defined group name
    virtual QString getGroupName() const;
    //!Returns server-defined group name
    QString getDefaultGroupName() const;
    virtual void setGroupName(const QString& value);
    //!Set group name (the one is show to the user in client)
    /*!
        This name is set by user.
        \a setGroupName name is generally set automatically (e.g., by server)
    */
    void setUserDefinedGroupName( const QString& value );
    virtual QString getGroupId() const;
    virtual void setGroupId(const QString& value);

    virtual QString getSharedId() const;

    void setScheduleDisabled(bool value);
    bool isScheduleDisabled() const;

    /** Check if a license is used for the current camera. */
    bool isLicenseUsed() const;
    void setLicenseUsed(bool value);

    Qn::FailoverPriority failoverPriority() const;
    void setFailoverPriority(Qn::FailoverPriority value);

    bool isAudioEnabled() const;
    bool isAudioForced() const;
    void setAudioEnabled(bool value);

    bool isAdvancedWorking() const;
    void setAdvancedWorking(bool value);

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

    QString getVendor() const;
    void setVendor(const QString &value);

    bool isGroupPlayOnly() const;

    //!Implementation of QnMediaResource::toResource
    virtual const QnResource* toResource() const override;
    //!Implementation of QnMediaResource::toResource
    virtual QnResource* toResource() override;
    //!Implementation of QnMediaResource::toResource
    virtual const QnResourcePtr toResourcePtr() const override;
    //!Implementation of QnMediaResource::toResource
    virtual QnResourcePtr toResourcePtr() override;
    void setSecondaryStreamQuality(Qn::SecondStreamQuality  quality);
    Qn::SecondStreamQuality  secondaryStreamQuality() const;

    void setCameraControlDisabled(bool value);
    bool isCameraControlDisabled() const;

    int desiredSecondStreamFps() const;
    Qn::StreamQuality getSecondaryStreamQuality() const;

    // TODO: #2.4 #rvasilenko #High Move to runtime data
    Qn::CameraStatusFlags statusFlags() const;
    bool hasStatusFlags(Qn::CameraStatusFlag value) const;
    void setStatusFlags(Qn::CameraStatusFlags value);
    void addStatusFlags(Qn::CameraStatusFlag value);
    void removeStatusFlags(Qn::CameraStatusFlag value);

    bool needCheckIpConflicts() const;

    void setMaxDays(int value);
    int maxDays() const;

    void setMinDays(int value);
    int minDays() const;

    void setPreferredServerId(const QnUuid& value);
    QnUuid preferredServerId() const;

    nx::api::AnalyticsSupportedEvents analyticsSupportedEvents() const;
    void setAnalyticsSupportedEvents(const nx::api::AnalyticsSupportedEvents& eventsList);

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
        int detailLevel );

    // in some cases I just want to update couple of field from just discovered resource
    virtual bool mergeResourcesIfNeeded(const QnNetworkResourcePtr &source) override;

    // TODO: #rvasilenko #High #2.4 get rid of this temporary solution
    /**
     * Temporary solution to correctly detach desktop cameras when user reconnects from one server to another.
     * Implemented in QnDesktopCameraResource.
     */
    virtual bool isReadyToDetach() const {return true;}

    //!Set list of IO ports
    void setIOPorts(const QnIOPortDataList& ports);

    virtual bool setProperty(
		const QString &key,
		const QString &value,
		PropertyOptions options = DEFAULT_OPTIONS) override;

    virtual bool setProperty(
		const QString &key,
		const QVariant& value,
		PropertyOptions options = DEFAULT_OPTIONS) override;

    virtual bool removeProperty(const QString& key) override;

    //!Returns list if IO ports
    QnIOPortDataList getIOPorts() const;

    //!Returns list of IO ports's states
    virtual QnIOStateDataList ioStates() const { return QnIOStateDataList(); }

    virtual Qn::BitratePerGopType bitratePerGopType() const;

    // Allow getting multi video layout directly from a RTSP SDP info
    virtual bool allowRtspVideoLayout() const { return true; }

#ifdef ENABLE_DATA_PROVIDERS
    virtual QnAudioTransmitterPtr getAudioTransmitter();
#endif

    bool isEnoughFpsToRunSecondStream(int currentFps) const;
    virtual nx::core::resource::AbstractRemoteArchiveManager* remoteArchiveManager();

    virtual void analyticsEventStarted(const QString& caption, const QString& description);
    virtual void analyticsEventEnded(const QString& caption, const QString& description);

    virtual int suggestBitrateForQualityKbps(Qn::StreamQuality q, QSize resolution, int fps, Qn::ConnectionRole role = Qn::CR_Default) const;
    virtual int suggestBitrateKbps(const QSize& resolution, const QnLiveStreamParams& streamParams, Qn::ConnectionRole role) const;
    float rawSuggestBitrateKbps(Qn::StreamQuality quality, QSize resolution, int fps) const;

    virtual bool captureEvent(const nx::vms::event::AbstractEventPtr& event);
    virtual bool doesEventComeFromAnalyticsDriver(nx::vms::event::EventType eventType) const;

    /**
     * Update user password at the camera. This function is able to change password for existing user only.
     */
    virtual bool setCameraCredentialsSync(
        const QAuthenticator& auth, QString* outErrorString = nullptr);

public slots:
    virtual void inputPortListenerAttached();
    virtual void inputPortListenerDetached();

    virtual void recordingEventAttached();
    virtual void recordingEventDetached();

signals:
    void scheduleDisabledChanged(const QnResourcePtr &resource);
    void scheduleTasksChanged(const QnResourcePtr &resource);
    void groupIdChanged(const QnResourcePtr &resource);
    void groupNameChanged(const QnResourcePtr &resource);
    void motionRegionChanged(const QnResourcePtr &resource);
    void statusFlagsChanged(const QnResourcePtr &resource);
    void licenseUsedChanged(const QnResourcePtr &resource);
    void licenseTypeChanged(const QnResourcePtr &resource);
    void failoverPriorityChanged(const QnResourcePtr &resource);
    void backupQualitiesChanged(const QnResourcePtr &resource);

    void networkIssue(const QnResourcePtr&, qint64 timeStamp, nx::vms::event::EventReason reasonCode, const QString& reasonParamsEncoded);

    //!Emitted on camera input port state has been changed
    /*!
        \param resource Smart pointer to \a this
        \param inputPortID
        \param value true if input is connected, false otherwise
        \param timestamp MSecs since epoch, UTC
    */
    void cameraInput(
        const QnResourcePtr& resource,
        const QString& inputPortID,
        bool value,
        qint64 timestamp );

    void cameraOutput(
        const QnResourcePtr& resource,
        const QString& inputPortID,
        bool value,
        qint64 timestamp );

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

#ifdef ENABLE_DATA_PROVIDERS
    virtual QnAbstractStreamDataProvider* createDataProviderInternal(Qn::ConnectionRole role) override;
#endif

    virtual void initializationDone() override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() = 0;

    virtual void setMotionMaskPhysical(int channel) { Q_UNUSED(channel); }
    //!MUST be overridden for camera with input port. Default implementation does nothing
    /*!
        \warning Excess calls of this method is legal and MUST be correctly handled in implementation
        \return true, if async request has been started successfully
        \note \a completionHandler is not called yet (support will emerge in 2.4)
    */
    virtual bool startInputPortMonitoringAsync( std::function<void(bool)>&& completionHandler );
    //!MUST be overridden for camera with input port. Default implementation does nothing
    /*!
        \warning Excess calls of this method is legal and MUST be correctly handled in implementation
        \note This method has no right to fail
    */
    virtual void stopInputPortMonitoringAsync();
    virtual bool isInputPortMonitored() const;

    virtual Qn::LicenseType calculateLicenseType() const;
protected:
#ifdef ENABLE_DATA_PROVIDERS
    QnAudioTransmitterPtr m_audioTransmitter;
#endif
private:
    QnDataProviderFactory *m_dpFactory;
    QAtomicInt m_inputPortListenerCount;
    int m_recActionCnt;
    QString m_groupName;
    QString m_groupId;
    Qn::CameraStatusFlags m_statusFlags;
    bool m_manuallyAdded;
    QString m_model;
    QString m_vendor;
    CachedValue<Qn::LicenseType> m_cachedLicenseType;
    CachedValue<bool> m_cachedHasDualStreaming2;
    CachedValue<Qn::MotionTypes> m_cachedSupportedMotionType;
    CachedValue<Qn::CameraCapabilities> m_cachedCameraCapabilities;
    CachedValue<bool> m_cachedIsDtsBased;
    CachedValue<Qn::MotionType> m_motionType;
    CachedValue<bool> m_cachedIsIOModule;
    Qn::MotionTypes calculateSupportedMotionType() const;
    Qn::MotionType calculateMotionType() const;
    CachedValue<nx::api::AnalyticsSupportedEvents> m_cachedAnalyticsSupportedEvents;
    CachedValue<nx::media::CameraMediaCapability> m_cachedCameraMediaCapabilities;

private slots:
    void resetCachedValues();
};

Q_DECLARE_METATYPE(QnSecurityCamResourcePtr)
Q_DECLARE_METATYPE(QnSecurityCamResourceList)

#endif //sequrity_cam_resource_h_1239
