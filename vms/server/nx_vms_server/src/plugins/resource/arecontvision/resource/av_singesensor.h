#pragma once

#ifdef ENABLE_ARECONT

#include "av_resource.h"



class CLArecontSingleSensorResource : public QnPlAreconVisionResource
{
public:
    CLArecontSingleSensorResource(QnMediaServerModule* serverModule, const QString& name);
    bool getDescription();
protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider();


};

#endif
