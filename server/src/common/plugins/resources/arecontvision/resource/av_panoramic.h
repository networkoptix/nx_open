#ifndef av_panoramic_device_1820
#define av_panoramic_device_1820

#include "av_resource.h"


class CLArecontPanoramicDevice : public QnPlAreconVisionResource
{
public:
	CLArecontPanoramicDevice(int model);
	bool getDescription();
	virtual QnAbstractMediaStreamDataProvider* getDeviceStreamConnection();

	virtual bool hasTestPattern() const;

protected:
	virtual bool setParam(const QString& name, const QnValue& val);

	virtual bool setParam_special(const QString& name, const QnValue& val);

private:
	bool setResolution(bool full);
	bool setQulity(int q);

protected:
	bool m_hastestPattern;

};

#endif //av_panoramic_device_1820
