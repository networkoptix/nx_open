#ifndef sequrity_cam_resource_h_1239
#define sequrity_cam_resource_h_1239

#include <QRegion>
#include "media_resource.h"
#include "core/misc/schedule_task.h"
#include "motion_window.h"

class QnDataProviderFactory
{
public:
    virtual ~QnDataProviderFactory() {}

    virtual QnAbstractStreamDataProvider* createDataProviderInternal(QnResourcePtr res, QnResource::ConnectionRole role) = 0;
};

enum MotionType {MT_Default = 0, MT_HardwareGrid = 1, MT_SoftwareGrid = 2, MT_MotionWindow = 4, MT_NoMotion = 8};

enum StreamFpsSharingMethod 
{
    shareFps, // if second stream is running whatever fps it has => first stream can get maximumFps - secondstreamFps
    sharePixels, //if second stream is running whatever megapixel it has => first stream can get maxMegapixels - secondstreamPixels
    noSharing // second stream does not subtract first stream fps 
};

Q_DECLARE_FLAGS(MotionTypeFlags, MotionType);

class QnSecurityCamResource : virtual public QnMediaResource
{
    Q_OBJECT

public:
    enum CameraCapability { 
        NoCapabilities = 0, 
        PtzCapability = 1, 
        ZoomCapability = 2, 
        PrimaryStreamSoftMotionCapability = 4,
        relayInput = 0x08,
        relayOutput = 0x10
    };
    Q_DECLARE_FLAGS(CameraCapabilities, CameraCapability)

    MotionTypeFlags supportedMotionType() const;
    bool isAudioSupported() const;
    MotionType getCameraBasedMotionType() const;
    MotionType getDefaultMotionType() const;
    int motionWindowCount() const;
    int motionMaskWindowCount() const;
    int motionSensWindowCount() const;


    MotionType getMotionType();
    void setMotionType(MotionType value);

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

    virtual StreamFpsSharingMethod streamFpsSharingMethod() const;

    virtual QStringList getRelayOutputList() const;
    virtual QStringList getInputPortList() const;


    CameraCapabilities getCameraCapabilities() const;
    void setCameraCapabilities(CameraCapabilities capabilities);
    void setCameraCapability(CameraCapability capability, bool value);


    /*!
        Change output with id \a ouputID state to \a activate
        \param autoResetTimeoutMS If > 0 and \a activate is \a true, than output will be deactivated in \a autoResetTimeout milliseconds
        \return true in case of success. false, if nothing has been done
    */
    virtual bool setRelayOutputState(const QString& ouputID, bool activate, unsigned int autoResetTimeoutMS = 0);

public slots:
    void inputPortListenerAttached();
    void inputPortListenerDetached();

signals:
    /** 
     * This signal is virtual to work around a problem with inheritance from
     * two <tt>QObject</tt>s. 
     */
    virtual void scheduleTasksChanged(const QnSecurityCamResourcePtr &resource);
    
    virtual void cameraCapabilitiesChanged(const QnSecurityCamResourcePtr &resource);

private slots:
    void at_disabledChanged();

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
    MotionType m_motionType;
    QAtomicInt m_inputPortListenerCount;
};

Q_DECLARE_METATYPE(QnSecurityCamResourcePtr)
Q_DECLARE_METATYPE(QnSecurityCamResourceList)

#endif //sequrity_cam_resource_h_1239
