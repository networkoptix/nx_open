#ifndef sequrity_cam_resource_h_1239
#define sequrity_cam_resource_h_1239

#include <QRegion>


#include "media_resource.h"
#include "motion_window.h"
#include "core/misc/schedule_task.h"

class QnDataProviderFactory {
public:
    virtual ~QnDataProviderFactory() {}

    virtual QnAbstractStreamDataProvider* createDataProviderInternal(QnResourcePtr res, QnResource::ConnectionRole role) = 0;
};


class QnSecurityCamResource : virtual public QnMediaResource {
    Q_OBJECT

public:
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

    // like arecont or iqinvision
    virtual QString manufacture() const = 0; // TODO: rename to manufacturer()
    virtual QString oemName() const;


    virtual int getMaxFps(); // in fact this is const function;

    virtual int reservedSecondStreamFps();  // in fact this is const function;

    virtual QSize getMaxSensorSize(); // in fact this is const function;

    virtual void setIframeDistance(int frames, int timems) = 0; // sets the distance between I frames

    virtual QRect getCroping(QnDomain domain); // TODO: 'cropping' is spelled with double 'p'. Rename
    virtual void setCroping(QRect croping, QnDomain domain); // sets cropping. rect is in the percents from 0 to 100

    void setDataProviderFactory(QnDataProviderFactory* dpFactory);

    void setMotionRegionList(const QList<QnMotionRegion>& maskList, QnDomain domain);
    void setMotionRegion(const QnMotionRegion& mask, QnDomain domain, int channel);

    QRegion getMotionMask(int channel) const;
    QnMotionRegion getMotionRegion(int channel) const;
    QList<QnMotionRegion> getMotionRegionList() const;

    void setScheduleTasks(const QnScheduleTaskList &scheduleTasks);
    const QnScheduleTaskList getScheduleTasks() const;

    virtual bool hasDualStreaming() const;

    virtual Qn::StreamFpsSharingMethod streamFpsSharingMethod() const;

    virtual QStringList getRelayOutputList() const;
    virtual QStringList getInputPortList() const;


    Qn::CameraCapabilities getCameraCapabilities() const;
    void setCameraCapabilities(Qn::CameraCapabilities capabilities);
    void setCameraCapability(Qn::CameraCapability capability, bool value);


    /*!
        Change output with id \a ouputID state to \a activate
        \param autoResetTimeoutMS If > 0 and \a activate is \a true, than output will be deactivated in \a autoResetTimeout milliseconds
        \return true in case of success. false, if nothing has been done
    */
    virtual bool setRelayOutputState(const QString& ouputID, bool activate, unsigned int autoResetTimeoutMS = 0);

    bool isRecordingEventAttached() const;

// -------------------------------------------------------------------------- //
// Begin QnSecurityCamResource signals/slots
// -------------------------------------------------------------------------- //
    /* For metaobject system to work correctly, this block must be in sync with
     * the one in QnVirtualCameraResource. */
public slots:
    virtual void inputPortListenerAttached();
    virtual void inputPortListenerDetached();

    virtual void recordingEventAttached();
    virtual void recordingEventDetached();

signals:
    virtual void scheduleTasksChanged(const QnSecurityCamResourcePtr &resource);
    virtual void cameraCapabilitiesChanged(const QnSecurityCamResourcePtr &resource);

protected slots:
    virtual void at_disabledChanged();
// -------------------------------------------------------------------------- //
// End QnSecurityCamResource signals/slots
// -------------------------------------------------------------------------- //

protected:
    void updateInner(QnResourcePtr other) override;

    virtual QnAbstractStreamDataProvider* createDataProviderInternal(QnResource::ConnectionRole role);

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() = 0;
    virtual void setCropingPhysical(QRect croping) = 0; // TODO: 'cropping'!!!
    virtual void setMotionMaskPhysical(int channel) { Q_UNUSED(channel); }
    //!MUST be overridden for camera with input port. Default implementation does noting
    virtual bool startInputPortMonitoring();
    //!MUST be overridden for camera with input port. Default implementation does noting
    virtual void stopInputPortMonitoring();

protected:
    QList<QnMotionRegion> m_motionMaskList;

private:
    QnDataProviderFactory *m_dpFactory;
    
    QnScheduleTaskList m_scheduleTasks;
    Qn::MotionType m_motionType;
    QAtomicInt m_inputPortListenerCount;
    int m_recActionCnt;
};

Q_DECLARE_METATYPE(QnSecurityCamResourcePtr)
Q_DECLARE_METATYPE(QnSecurityCamResourceList)

#endif //sequrity_cam_resource_h_1239
