#ifndef av_panoramic_device_1820
#define av_panoramic_device_1820

#include "av_resource.h"


class CLArecontPanoramicResource : public QnPlAreconVisionResource
{
public:
	CLArecontPanoramicResource(const QString& name);
	bool getDescription();

protected:

    virtual bool setParamPhysical(const QString& name, const QnValue& val);
    virtual bool setSpecialParam(const QString& name, const QnValue& val, QnDomain domain);

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

private:
	bool setResolution(bool full);
	bool setCamQulity(int q);



};

#endif //av_panoramic_device_1820
