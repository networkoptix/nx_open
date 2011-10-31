#ifndef av_singlesensor_h_1252
#define av_singlesensor_h_1252

#include "av_resource.h"



class CLArecontSingleSensorDevice : public QnPlAreconVisionResource
{
public:
	CLArecontSingleSensorDevice(const QString& name);
	bool getDescription();

	virtual bool hasTestPattern() const;
protected:
    virtual QnAbstractStreamDataProvider* createDataProviderInternal(ConnectionRole role);
protected:
	bool m_hastestPattern;

};

#endif // av_singlesensor_h_1252
