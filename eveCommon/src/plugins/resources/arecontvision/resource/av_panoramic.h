#ifndef av_panoramic_device_1820
#define av_panoramic_device_1820

#include "av_resource.h"


class CLArecontPanoramicDevice : public QnPlAreconVisionResource
{
public:
	CLArecontPanoramicDevice(const QString& name);
	bool getDescription();
	virtual QnAbstractMediaStreamDataProvider* createDataProviderInternal();

	virtual bool hasTestPattern() const;
protected:

    virtual bool setParamPhysical(const QString& name, const QnValue& val);
    virtual bool setSpecialParam(const QString& name, const QnValue& val, QnDomain domain);

private:
	bool setResolution(bool full);
	bool setCamQulity(int q);

protected:
	bool m_hastestPattern;

};

#endif //av_panoramic_device_1820
