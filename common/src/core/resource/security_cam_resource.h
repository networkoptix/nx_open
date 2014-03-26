#ifndef sequrity_cam_resource_h_1239
#define sequrity_cam_resource_h_1239

#include <QtGui/QRegion>
#include <QtCore/QMutex>

#include "media_resource.h"
#include "motion_window.h"
#include "core/misc/schedule_task.h"
#include "network_resource.h"

class QnAbstractArchiveDelegate;

class QnDataProviderFactory {
public:
    virtual ~QnDataProviderFactory() {}

    virtual QnAbstractStreamDataProvider* createDataProviderInternal(QnResourcePtr res, QnResource::ConnectionRole role) = 0;
};


class QnSecurityCamResource : public QnNetworkResource, public QnMediaResource {
    typedef QnNetworkResource base_type;
    Q_OBJECT

public:
    enum StatusFlag {
        HasIssuesFlag = 0x1
    };
    Q_DECLARE_FLAGS(StatusFlags, StatusFlag)

    Qn::MotionTypes supportedMotionType() const;
    bool isAudioSupported() const;
    Qn::MotionType getCameraBasedMotionType() const;
    Qn::MotionType getDefaultMotionType() const;
    int motionWindowCount() const;
    int motionMaskWindowCount() const;
    int motionSensWindowCount() const;


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

    void setScheduleTasks(const QnScheduleTaskList &scheduleTasks);
    const QnScheduleTaskList getScheduleTasks() const;

    virtual bool hasDualStreaming() const;

    /** Return true if dual streaming supported and don't blocked by user */
    bool hasDualStreaming2() const;

    /** Returns true if camera stores archive on a external system */
    bool isDtsBased() const;

    /** Returns true if it is a analog camera */
    bool isAnalog() const;

    virtual Qn::StreamFpsSharingMethod streamFpsSharingMethod() const;

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

    StatusFlags statusFlags() const;
    bool hasStatusFlags(StatusFlags value) const;
    void setStatusFlags(StatusFlags value);
    void addStatusFlags(StatusFlags value);
    void removeStatusFlags(StatusFlags value);

    bool needCheckIpConflicts() const;

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

    //!Emitted on camera input port state has been changed
    /*!
        \param resource Smart pointer to \a this
        \param inputPortID
        \param value true if input is connected, false otherwise
        \param timestamp MSecs since epoch, UTC
    */
    void cameraInput(
        QnResourcePtr resource,
        const QString& inputPortID,
        bool value,
        qint64 timestamp );

protected slots:
    virtual void at_disabledChanged();

protected:
    void updateInner(QnResourcePtr other) override;

    virtual QnAbstractStreamDataProvider* createDataProviderInternal(QnResource::ConnectionRole role) override;
    virtual void initializationDone() override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() = 0;

    virtual void setMotionMaskPhysical(int channel) { Q_UNUSED(channel); }
    //!MUST be overridden for camera with input port. Default implementation does noting
    /*!
        Excess calls of this method is legal and MUST be correctly handled in implementation
    */
    virtual bool startInputPortMonitoring();
    //!MUST be overridden for camera with input port. Default implementation does noting
    /*!
        Excess calls of this method is legal and MUST be correctly handled in implementation
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
    StatusFlags m_statusFlags;
    bool m_scheduleDisabled;
    bool m_audioEnabled;
    bool m_advancedWorking;
    bool m_manuallyAdded;
    QString m_model;
    QString m_vendor;
    QString m_firmware;
};

Q_DECLARE_METATYPE(QnSecurityCamResourcePtr)
Q_DECLARE_METATYPE(QnSecurityCamResourceList)

#endif //sequrity_cam_resource_h_1239
