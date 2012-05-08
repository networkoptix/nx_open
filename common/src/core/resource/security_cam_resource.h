#ifndef sequrity_cam_resource_h_1239
#define sequrity_cam_resource_h_1239

#include <QRegion>
#include "media_resource.h"
#include "core/misc/scheduleTask.h"
#include "motion_window.h"

class QnDataProviderFactory
{
public:
    virtual ~QnDataProviderFactory() {}

    virtual QnAbstractStreamDataProvider* createDataProviderInternal(QnResourcePtr res, QnResource::ConnectionRole role) = 0;
};

enum MotionType {MT_Default = 0, MT_HardwareGrid = 1, MT_SoftwareGrid = 2, MT_MotionWindow = 4, MT_NoMotion = 8};
Q_DECLARE_FLAGS(MotionTypeFlags, MotionType);

class QnSecurityCamResource : virtual public QnMediaResource
{
    Q_OBJECT

public:

    MotionTypeFlags supportedMotionType() const;
    MotionType getDefaultMotionType() const;
    int motionWindowCnt() const; // TODO: 'cnt' reads as 'cunt', and adequate people normally don't want cunts in their code. Rename.
    int motionMaskWindowCnt() const;


    MotionType getMotionType();
    void setMotionType(MotionType value);

    QnSecurityCamResource();
    virtual ~QnSecurityCamResource();

    // like arecont or iqinvision
    virtual QString manufacture() const = 0; // TODO: rename to manufacturer()
    virtual QString oemName() const;


    virtual int getMaxFps(); // in fact this is const function;

    virtual QSize getMaxSensorSize(); // in fact this is const function;

    virtual void setIframeDistance(int frames, int timems) = 0; // sets the distance between I frames

    virtual QRect getCroping(QnDomain domain); // TODO: 'cropping' is spelled with double 'p'. Rename
    virtual void setCroping(QRect croping, QnDomain domain); // sets croping. rect is in the percents from 0 to 100

    void setDataProviderFactory(QnDataProviderFactory* dpFactory);

    void setMotionRegionList(const QList<QnMotionRegion>& maskList, QnDomain domain);
    void setMotionRegion(const QnMotionRegion& mask, QnDomain domain, int channel);
    
    QRegion getMotionMask(int channel) const;
    QnMotionRegion getMotionRegion(int channel) const;
    QList<QnMotionRegion> getMotionRegionList() const;

    void setScheduleTasks(const QnScheduleTaskList &scheduleTasks);
    const QnScheduleTaskList &getScheduleTasks() const;

    virtual bool hasDualStreaming() const;

protected:
    void updateInner(QnResourcePtr other) override;

    virtual QnAbstractStreamDataProvider* createDataProviderInternal(QnResource::ConnectionRole role);

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() = 0;
    virtual void setCropingPhysical(QRect croping) = 0; // TODO: 'cropping'!!!
    virtual void setMotionMaskPhysical(int channel) { Q_UNUSED(channel); }

protected:
    QList<QnMotionRegion> m_motionMaskList;
private:
    QnDataProviderFactory* m_dpFactory;
    
    QnScheduleTaskList m_scheduleTasks;
    MotionType m_motionType;
};

Q_DECLARE_METATYPE(QnMotionRegion)

#endif //sequrity_cam_resource_h_1239
