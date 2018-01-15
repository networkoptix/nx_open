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

    bool getParamPhysicalByChannel(int channel, const QString& name, QString &val);

    void updateFlipState();
protected:
    virtual nx::mediaserver::resource::StreamCapabilityMap getStreamCapabilityMapFromDrive(bool primaryStream) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;

    virtual bool setApiParameter(const QString &id, const QString &value) override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(const QnAbstractStreamDataProvider* dataProvider) const override;
private:
    bool setSpecialParam(const QString& id, const QString& value);
    bool setResolution(bool full);
    bool setCamQuality(int q);
    QnConstResourceVideoLayoutPtr getDefaultVideoLayout() const;
    int getChannelCount() const;
protected:
    bool m_isRotated;
    mutable QSharedPointer<QnCustomResourceVideoLayout> m_customVideoLayout;
    mutable QnResourceVideoLayoutPtr m_defaultVideoLayout;
    mutable QnMutex m_layoutMutex;
};

typedef QnSharedResourcePointer<QnArecontPanoramicResource> QnArecontPanoramicResourcePtr;

#endif

#endif //av_panoramic_device_1820
