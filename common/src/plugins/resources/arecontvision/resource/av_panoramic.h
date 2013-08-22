#ifndef av_panoramic_device_1820
#define av_panoramic_device_1820

#include "av_resource.h"


class QnArecontPanoramicResource : public QnPlAreconVisionResource
{
public:
    QnArecontPanoramicResource(const QString& name);
    bool getDescription();

    bool getParamPhysical(int cannel, const QString& name, QVariant &val);

    void updateFlipState();
protected:
    virtual CameraDiagnostics::Result initInternal() override;

    virtual bool setParamPhysical(const QnParam &param, const QVariant &val) override;
    virtual bool setSpecialParam(const QString& name, const QVariant& val, QnDomain domain);

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual const QnResourceVideoLayout* getVideoLayout(const QnAbstractStreamDataProvider* dataProvider) override;
private:
    bool setResolution(bool full);
    bool setCamQuality(int q);

protected:
    QnResourceVideoLayout* m_vrl;
    bool m_isRotated;    
};

typedef QnSharedResourcePointer<QnArecontPanoramicResource> QnArecontPanoramicResourcePtr;

#endif //av_panoramic_device_1820
