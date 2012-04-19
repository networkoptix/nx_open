#ifndef sequrity_cam_resource_h_1239
#define sequrity_cam_resource_h_1239

#include <QRegion>
#include "media_resource.h"
#include "core/misc/scheduleTask.h"

class QnDataProviderFactory
{
public:
    virtual ~QnDataProviderFactory() {}

    virtual QnAbstractStreamDataProvider* createDataProviderInternal(QnResourcePtr res, QnResource::ConnectionRole role) = 0;
};

enum MotionType {MT_NoMotion = 0, MT_HardwareGrid = 1, MT_SoftwareGrid = 2, MT_MotionWindow = 4};
Q_DECLARE_FLAGS(MotionTypeFlags, MotionType);

class QnSecurityCamResource : virtual public QnMediaResource
{
    Q_OBJECT

public:

    MotionTypeFlags supportedMotionType();

    QnSecurityCamResource();
    virtual ~QnSecurityCamResource();

    // like arecont or iqinvision
    virtual QString manufacture() const = 0;
    virtual QString oemName() const;


    virtual int getMaxFps(); // in fact this is const function;

    virtual QSize getMaxSensorSize(); // in fact this is const function;

    virtual void setIframeDistance(int frames, int timems) = 0; // sets the distance between I frames

    virtual QRect getCroping(QnDomain domain);
    virtual void setCroping(QRect croping, QnDomain domain); // sets croping. rect is in the percents from 0 to 100

    void setDataProviderFactory(QnDataProviderFactory* dpFactory);

    void setMotionMaskList(const QList<QRegion>& maskList, QnDomain domain);
    void setMotionMask(const QRegion& mask, QnDomain domain, int channel);
    QRegion getMotionMask(int channel) const;
    QList<QRegion> getMotionMaskList() const;

    void setScheduleTasks(const QnScheduleTaskList &scheduleTasks);
    const QnScheduleTaskList &getScheduleTasks() const;

    virtual bool hasDualStreaming() const;

protected:
    void updateInner(QnResourcePtr other) override;

    virtual QnAbstractStreamDataProvider* createDataProviderInternal(QnResource::ConnectionRole role);

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() = 0;
    virtual void setCropingPhysical(QRect croping) = 0;
    virtual void setMotionMaskPhysical(int channel){Q_UNUSED(channel);};

protected:
    QList<QRegion> m_motionMaskList;
private:
    QnDataProviderFactory* m_dpFactory;
    
    QnScheduleTaskList m_scheduleTasks;
};

Q_DECLARE_METATYPE(QRegion)

#endif //sequrity_cam_resource_h_1239
