#ifndef av_panoramic_device_1820
#define av_panoramic_device_1820

#ifdef ENABLE_ARECONT

#include <QElapsedTimer>
#include "av_resource.h"


class QnArecontPanoramicResource : public QnPlAreconVisionResource
{
public:
    QnArecontPanoramicResource(const QString& name);
    ~QnArecontPanoramicResource();
    bool getDescription();

    bool getParamPhysical(int channel, const QString& name, QVariant &val);

    void updateFlipState();
protected:
    virtual CameraDiagnostics::Result initInternal() override;

    virtual bool setParamPhysical(const QnParam &param, const QVariant &val) override;
    virtual bool setSpecialParam(const QString& name, const QVariant& val, QnDomain domain);

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(const QnAbstractStreamDataProvider* dataProvider) const override;
private:
    bool setResolution(bool full);
    bool setCamQuality(int q);

protected:
    QnResourceVideoLayoutPtr m_vrl;
    bool m_isRotated;    
    mutable QnCustomResourceVideoLayoutPtr m_rotatedLayout;
    QElapsedTimer m_flipTimer;
};

typedef QnSharedResourcePointer<QnArecontPanoramicResource> QnArecontPanoramicResourcePtr;

#endif

#endif //av_panoramic_device_1820
