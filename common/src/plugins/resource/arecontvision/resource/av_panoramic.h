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
    virtual bool isAbstractResource() const override { return false; }
protected:
    virtual CameraDiagnostics::Result initInternal() override;

    virtual bool setParamPhysical(const QString &id, const QString &value) override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(const QnAbstractStreamDataProvider* dataProvider) const override;
private:
    bool setSpecialParam(const QString& id, const QString& value);
    bool setResolution(bool full);
    bool setCamQuality(int q);
    QnConstResourceVideoLayoutPtr getDefaultVideoLayout() const;
protected:
    QnResourceVideoLayoutPtr m_vrl;
    bool m_isRotated;    
    mutable QnCustomResourceVideoLayoutPtr m_rotatedLayout;
    QElapsedTimer m_flipTimer;
    mutable QnResourceVideoLayoutPtr m_defaultVideoLayout;
};

typedef QnSharedResourcePointer<QnArecontPanoramicResource> QnArecontPanoramicResourcePtr;

#endif

#endif //av_panoramic_device_1820
