#ifndef av_singlesensor_h_1252
#define av_singlesensor_h_1252

#include "av_resource.h"



class CLArecontSingleSensorDevice : public QnPlAreconVisionResource
{
public:
	CLArecontSingleSensorDevice(const QString& name);
	bool getDescription();
	virtual QnAbstractMediaStreamDataProvider* createMediaProvider();

	virtual bool hasTestPattern() const;

protected:
	bool m_hastestPattern;

};

#endif // av_singlesensor_h_1252
