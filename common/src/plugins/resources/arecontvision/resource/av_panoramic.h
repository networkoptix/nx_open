#ifndef av_panoramic_device_1820
#define av_panoramic_device_1820

#ifdef ENABLE_ARECONT

#include "av_resource.h"


class QnArecontPanoramicResource : public QnPlAreconVisionResource
{
public:
    QnArecontPanoramicResource(const QString& name);
    ~QnArecontPanoramicResource();
    bool getDescription();

    bool getParamPhysical(int cannel, const QString& name, QVariant &val);

    void updateFlipState();
protected:
    virtual CameraDiagnostics::Result initInternal() override;

    virtual bool setParamPhysical(const QnParam &param, const QVariant &val) override;
    virtual bool setSpecialParam(const QString& name, const QVariant& val, QnDomain domain);

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(const QnAbstractStreamDataProvider* dataProvider) override;
private:
    bool setResolution(bool full);
    bool setCamQuality(int q);

protected:
    QnResourceVideoLayoutPtr m_vrl;
    bool m_isRotated;    
    QnCustomResourceVideoLayoutPtr m_rotatedLayout;
};

typedef QnSharedResourcePointer<QnArecontPanoramicResource> QnArecontPanoramicResourcePtr;

#endif

#endif //av_panoramic_device_1820
