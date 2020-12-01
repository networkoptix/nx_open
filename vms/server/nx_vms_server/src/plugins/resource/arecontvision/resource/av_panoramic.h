#ifndef av_panoramic_device_1820
#define av_panoramic_device_1820

#ifdef ENABLE_ARECONT

#include <QElapsedTimer>
#include "av_resource.h"


class QnArecontPanoramicResource : public QnPlAreconVisionResource
{
public:
    QnArecontPanoramicResource(QnMediaServerModule* serverModule, const QString& name);
    ~QnArecontPanoramicResource();
    bool getDescription();

    bool getParamPhysicalByChannel(int channel, const QString& name, QString &val);

    void updateFlipState();
protected:
    virtual CameraDiagnostics::Result initializeCameraDriver() override;

    virtual bool setApiParameter(const QString &id, const QString &value) override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(const QnAbstractStreamDataProvider* dataProvider) override;
private:
    bool setSpecialParam(const QString& id, const QString& value);
    bool setResolution(bool full);
    bool setCamQuality(int q);
    QnConstResourceVideoLayoutPtr getDefaultVideoLayout() const;
    void initializeVideoLayoutUnsafe() const;
    int getChannelCount();
protected:
    bool m_isRotated;
    mutable QSharedPointer<QnCustomResourceVideoLayout> m_customVideoLayout;
    mutable QnResourceVideoLayoutPtr m_defaultVideoLayout;
    mutable QnMutex m_layoutMutex;
};

typedef QnSharedResourcePointer<QnArecontPanoramicResource> QnArecontPanoramicResourcePtr;

#endif

#endif //av_panoramic_device_1820
