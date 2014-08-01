#ifndef av_singlesensor_h_1252
#define av_singlesensor_h_1252

#ifdef ENABLE_ARECONT

#include "av_resource.h"



class CLArecontSingleSensorResource : public QnPlAreconVisionResource
{
public:
    CLArecontSingleSensorResource(const QString& name);
    bool getDescription();
    virtual bool isAbstractResource() const override { return false; }
protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider();
    

};

#endif

#endif // av_singlesensor_h_1252
