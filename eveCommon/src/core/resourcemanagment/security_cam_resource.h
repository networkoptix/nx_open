#ifndef sequrity_cam_resource_h_1239
#define sequrity_cam_resource_h_1239

#include <QtCore/QRect>
#include "../resource/media_resource.h"

class QnDataProviderFactory
{
public:
    virtual QnAbstractStreamDataProvider* createDataProviderInternal(QnResourcePtr res, QnResource::ConnectionRole role) = 0;
};

class QnSequrityCamResource : virtual public QnMediaResource
{
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

protected:
    virtual QnAbstractStreamDataProvider* createDataProviderInternal(QnResource::ConnectionRole role);

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() = 0;
    virtual void setCropingPhysical(QRect croping) = 0;
private:
    QnDataProviderFactory* m_dpFactory;
};

typedef QSharedPointer<QnSequrityCamResource> QnSequrityCamResourcePtr;

#endif //sequrity_cam_resource_h_1239
