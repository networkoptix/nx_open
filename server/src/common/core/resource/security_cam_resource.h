#ifndef sequrity_cam_resource_h_1239
#define sequrity_cam_resource_h_1239

#include "media_resource.h"

class QnSequrityCamResource : virtual public QnMediaResource
{
public:
    QnSequrityCamResource();
    virtual ~QnSequrityCamResource();

    virtual int getMaxFps(); // in fact this is const function; 

    virtual QSize getMaxSensorSize(); // in fact this is const function; 

    virtual void setIframeDistance(int frames, int timems) = 0; // sets the distance between I frames 

    virtual QRect getCroping(QnDomain domain);
    virtual void setCroping(QRect croping, QnDomain domain); // sets croping. rect is in the percents from 0 to 100
    
protected:

    virtual void setCropingPhysical(QRect croping) = 0;

};

#endif //sequrity_cam_resource_h_1239