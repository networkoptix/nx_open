#ifndef av_panoramic_device_1820
#define av_panoramic_device_1820

#include "av_resource.h"


class QnArecontPanoramicResource : public QnPlAreconVisionResource
{
public:
    QnArecontPanoramicResource(const QString& name);
    bool getDescription();

    virtual bool hasDualStreaming() const override;

    virtual void init() override;

protected:

    virtual bool setParamPhysical(const QnParam &param, const QVariant &val);
    virtual bool setSpecialParam(const QString& name, const QVariant& val, QnDomain domain);

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

private:
    bool setResolution(bool full);
    bool setCamQuality(int q);

protected:
    QnVideoResourceLayout* m_vrl;
};

#endif //av_panoramic_device_1820
