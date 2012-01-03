#ifndef av_panoramic_device_1820
#define av_panoramic_device_1820

#include "av_resource.h"


class CLArecontPanoramicResource : public QnPlAreconVisionResource
{
public:
	CLArecontPanoramicResource(const QString& name);
	bool getDescription();

    virtual const QnVideoResourceLayout* getVideoLayout(const QnAbstractMediaStreamDataProvider* dataProvider = 0) override;
protected:

    virtual bool setParamPhysical(const QString& name, const QVariant& val);
    virtual bool setSpecialParam(const QString& name, const QVariant& val, QnDomain domain);

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

private:
	bool setResolution(bool full);
	bool setCamQulity(int q);

protected:
    QnVideoResourceLayout* m_vrl;
};

#endif //av_panoramic_device_1820
