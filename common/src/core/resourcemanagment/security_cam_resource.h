#ifndef sequrity_cam_resource_h_1239
#define sequrity_cam_resource_h_1239

#include <QRegion>
#include "../resource/media_resource.h"
#include "core/misc/scheduleTask.h"

class QnDataProviderFactory
{
public:
    virtual QnAbstractStreamDataProvider* createDataProviderInternal(QnResourcePtr res, QnResource::ConnectionRole role) = 0;
};

class QnSequrityCamResource : virtual public QnMediaResource
{
    Q_OBJECT
public:
    QnSequrityCamResource();
    virtual ~QnSequrityCamResource();

    // like arecont or iqinvision
    virtual QString manufacture() const = 0;
    virtual QString oemName() const;


    virtual int getMaxFps(); // in fact this is const function;

    virtual QSize getMaxSensorSize(); // in fact this is const function;

    virtual void setIframeDistance(int frames, int timems) = 0; // sets the distance between I frames

    virtual QRect getCroping(QnDomain domain);
    virtual void setCroping(QRect croping, QnDomain domain); // sets croping. rect is in the percents from 0 to 100

    void setDataProviderFactory(QnDataProviderFactory* dpFactory);

    void setMotionMask(const QRegion& mask);
    QRegion getMotionMask() const;

    void setScheduleTasks(const QnScheduleTaskList& scheduleTasks);
    const QnScheduleTaskList getScheduleTasks() const;

    QnSequrityCamResource& operator=(const QnResource& other) override;

signals:
    void motionMaskChanged(QRegion region);

protected:
    virtual QnAbstractStreamDataProvider* createDataProviderInternal(QnResource::ConnectionRole role);

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() = 0;
    virtual void setCropingPhysical(QRect croping) = 0;
private:
    QnDataProviderFactory* m_dpFactory;
    QRegion m_motionMask;
    QnScheduleTaskList m_scheduleTasks;
};

typedef QSharedPointer<QnSequrityCamResource> QnSequrityCamResourcePtr;
typedef QList<QnSequrityCamResourcePtr> QnSequrityCamResourceList;

#endif //sequrity_cam_resource_h_1239
