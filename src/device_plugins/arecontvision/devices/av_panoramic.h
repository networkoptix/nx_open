#ifndef av_panoramic_device_1820
#define av_panoramic_device_1820


#include "av_device.h"


class CLArecontPanoramicDevice : public CLAreconVisionDevice
{
public:
	CLArecontPanoramicDevice(int model);
	bool getDescription();
	virtual CLStreamreader* getDeviceStreamConnection();

	bool getBaseInfo();
	QString getFullName() ;

	virtual bool hasTestPattern() const;

protected:
	bool m_hastestPattern;

};


#endif //av_panoramic_device_1820