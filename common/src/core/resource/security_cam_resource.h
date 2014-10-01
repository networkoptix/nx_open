#ifndef sequrity_cam_resource_h_1239
#define sequrity_cam_resource_h_1239

#include <QtGui/QRegion>
#include <QtCore/QMutex>

#include "media_resource.h"
#include "motion_window.h"
#include "core/misc/schedule_task.h"
#include "network_resource.h"
#include "common/common_globals.h"
#include "business/business_fwd.h"

class QnAbstractArchiveDelegate;

class QnDataProviderFactory {
public:
    virtual ~QnDataProviderFactory() {}

    virtual QnAbstractStreamDataProvider* createDataProviderInternal(const QnResourcePtr& res, Qn::ConnectionRole role) = 0;
};


class QnUserCameraSettings
{
};

class QnSecurityCamResource : public QnNetworkResource, public QnMediaResource {
    typedef QnNetworkResource base_type;
    Q_OBJECT

public:
    Qn::MotionTypes supportedMotionType() const;
    bool isAudioSupported() const;
    Qn::MotionType getCameraBasedMotionType() const;
    Qn::MotionType getDefaultMotionType() const;
    int motionWindowCount() const;
    int motionMaskWindowCount() const;
    int motionSensWindowCount() const;


    bool hasMotion();
    Qn::MotionType getMotionType();
    void setMotionType(Qn::MotionType value);

    QnSecurityCamResource();
    virtual ~QnSecurityCamResource();

    //!Returns driver (built-in or external) name, used to manage camera
    /*!
        This can be "axis", "dlink", "onvif", etc.
    */
    virtual QString getDriverName() const = 0;

    virtual int getMaxFps() const;

    virtual int reservedSecondStreamFps() const;

    virtual QSize getMaxSensorSize() const;

    virtual void setIframeDistance(int frames, int timems) = 0; // sets the distance between I frames

    void setDataProviderFactory(QnDataProviderFactory* dpFactory);

    QList<QnMotionRegion> getMotionRegionList() const;
    void setMotionRegionList(const QList<QnMotionRegion>& maskList, QnDomain domain);

    QnMotionRegion getMotionRegion(int channel) const;
    void setMotionRegion(const QnMotionRegion& mask, QnDomain domain, int channel);

    QRegion getMotionMask(int channel) const;

    //!Get camera settings, which are generally modified by user
    /*!
        E.g., recording schedule, motion type, second stream quality, etc...
    */
    QnUserCameraSettings getUserCameraSettings() const;

    void setScheduleTasks(const QnScheduleTaskList &scheduleTasks);
    const QnScheduleTaskList getScheduleTasks() const;

    virtual bool hasDualStreaming() const;

    /** Return true if dual streaming supported and don't blocked by user */
    bool hasDualStreaming2() const;

    /** Returns true if camera stores archive on a external system */
    bool isDtsBased() const;

    /** Returns true if it is a analog camera */
    bool isAnalog() const;

    /** Returns true if it is a analog encoder (described in resource_data.json) */
    bool isAnalogEncoder() const;


    /** Returns true if it is a edge camera */
    bool isEdge() const;

    /** Returns edge, analog or digital class */
    virtual Qn::LicenseType licenseType() const;



    virtual Qn::StreamFpsSharingMethod streamFpsSharingMethod() const;
    void setStreamFpsSharingMethod(Qn::StreamFpsSharingMethod value);

    virtual QStringList getRelayOutputList() const;
    virtual QStringList getInputPortList() const;


    Qn::CameraCapabilities getCameraCapabilities() const;
    bool hasCameraCapabilities(Qn::CameraCapabilities capabilities) const;
    void setCameraCapabilities(Qn::CameraCapabilities capabilities);
    void setCameraCapability(Qn::CameraCapability capability, bool value);


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

    virtual QString getGroupName() const;
    virtual void setGroupName(const QString& value);
    virtual QString getGroupId() const;
    virtual void setGroupId(const QString& value);

    void setScheduleDisabled(bool value);
    bool isScheduleDisabled() const;

    bool isAudioEnabled() const;
    void setAudioEnabled(bool value);

    bool isAdvancedWorking() const;
    void setAdvancedWorking(bool value);

    bool isManuallyAdded() const;
    void setManuallyAdded(bool value);

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

    void setPreferedServerId(const QnUuid& value);
    QnUuid preferedServerId() const;

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
    virtual bool mergeResourcesIfNeeded(const QnNetworkResourcePtr &source);

public slots:
    virtual void inputPortListenerAttached();
    virtual void inputPortListenerDetached();

    virtual void recordingEventAttached();
    virtual void recordingEventDetached();

signals:
    void scheduleDisabledChanged(const QnSecurityCamResourcePtr &resource);
    void scheduleTasksChanged(const QnSecurityCamResourcePtr &resource);
    void cameraCapabilitiesChanged(const QnSecurityCamResourcePtr &resource);
    void groupNameChanged(const QnSecurityCamResourcePtr &resource);
    void motionRegionChanged(const QnResourcePtr &resource);
    void networkIssue(const QnResourcePtr&, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString& reasonParamsEncoded);

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

protected slots:
    virtual void at_parentIdChanged();

protected:
    void updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) override;

    virtual QnAbstractStreamDataProvider* createDataProviderInternal(Qn::ConnectionRole role) override;
    virtual void initializationDone() override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() = 0;

    virtual void setMotionMaskPhysical(int channel) { Q_UNUSED(channel); }
    //!MUST be overridden for camera with input port. Default implementation does nothing
    /*!
        \warning Excess calls of this method is legal and MUST be correctly handled in implementation
        \return true, if started input port monitoring
    */
    virtual bool startInputPortMonitoring();
    //!MUST be overridden for camera with input port. Default implementation does nothing
    /*!
        \warning Excess calls of this method is legal and MUST be correctly handled in implementation
    */
    virtual void stopInputPortMonitoring();
    virtual bool isInputPortMonitored() const;

    virtual void parameterValueChangedNotify(const QnParam &param) override;

protected:
    QList<QnMotionRegion> m_motionMaskList;

private:
    QnDataProviderFactory *m_dpFactory;
    QnScheduleTaskList m_scheduleTasks;
    Qn::MotionType m_motionType;
    QAtomicInt m_inputPortListenerCount;
    int m_recActionCnt;
    QString m_groupName;
    QString m_groupId;
    Qn::SecondStreamQuality  m_secondaryQuality;
    bool m_cameraControlDisabled;
    Qn::CameraStatusFlags m_statusFlags;
    bool m_scheduleDisabled;
    bool m_audioEnabled;
    bool m_advancedWorking;
    bool m_manuallyAdded;
    QString m_model;
    QString m_vendor;
    int m_minDays;
    int m_maxDays;
    QnUuid m_preferedServerId;
};

Q_DECLARE_METATYPE(QnSecurityCamResourcePtr)
Q_DECLARE_METATYPE(QnSecurityCamResourceList)

#endif //sequrity_cam_resource_h_1239
