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

class QnSecurityCamResource : virtual public QnMediaResource
{
    Q_OBJECT

public:
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

    void setMotionMask(const QRegion& mask, QnDomain domain);
    QRegion getMotionMask() const;

    void setScheduleTasks(const QnScheduleTaskList &scheduleTasks);
    const QnScheduleTaskList &getScheduleTasks() const;

    virtual bool hasDualStreaming() const;

Q_SIGNALS:
    void motionMaskChanged(QRegion region);

protected:
    void updateInner(QnResourcePtr other) override;

    virtual QnAbstractStreamDataProvider* createDataProviderInternal(QnResource::ConnectionRole role);

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() = 0;
    virtual void setCropingPhysical(QRect croping) = 0;
    virtual void setMotionMaskPhysical(){};

protected:
    QRegion m_motionMask;
private:
    QnDataProviderFactory* m_dpFactory;
    
    QnScheduleTaskList m_scheduleTasks;
};

Q_DECLARE_METATYPE(QRegion)

#endif //sequrity_cam_resource_h_1239
