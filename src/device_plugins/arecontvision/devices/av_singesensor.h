#ifndef av_singlesensor_h_1252
#define av_singlesensor_h_1252


#include "av_device.h"

class CLArecontSingleSensorDevice : public CLAreconVisionDevice
{
public:
	CLArecontSingleSensorDevice(int model);
	bool getDescription();
	virtual CLStreamreader* getDeviceStreamConnection();

	bool getBaseInfo();
	QString getFullName() ;

	virtual bool hasTestPattern() const;

protected:
	bool m_hastestPattern;

};

#endif // av_singlesensor_h_1252