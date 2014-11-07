#ifndef QnPlAreconVisionResource_h_1252
#define QnPlAreconVisionResource_h_1252

#ifdef ENABLE_ARECONT

#include <QtGui/QImage>

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/media_data_packet.h"

class QDomElement;

// TODO: #Elric rename class to *ArecontVision*, rename file
class QnPlAreconVisionResource : public QnPhysicalCameraResource
{
    Q_OBJECT

public:
    static const QString MANUFACTURE;

    QnPlAreconVisionResource();

    CLHttpStatus getRegister(int page, int num, int& val);
    CLHttpStatus setRegister(int page, int num, int val);
    CLHttpStatus setRegister_asynch(int page, int num, int val);

    bool isPanoramic() const;
    bool isDualSensor() const;
    virtual bool isAbstractResource() const override;

    virtual void setHostAddress(const QString& ip) override;

    virtual bool getDescription() {return true;};

    //========
    bool unknownResource() const;
    virtual QnResourcePtr updateResource();
    //========

    //!Implementation of QnNetworkResource::ping
    virtual bool ping() override;

    virtual QString getDriverName() const override;

    virtual Qn::StreamQuality getBestQualityForSuchOnScreenSize(const QSize& size) const override;


    virtual QImage getImage(int channnel, QDateTime time, Qn::StreamQuality quality) const override;


    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    //virtual QnMediaInfo getMediaInfo() const;

    int totalMdZones() const;
    bool isH264() const;

protected:
    virtual CameraDiagnostics::Result initInternal() override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    // should just do physical job ( network or so ) do not care about memory domain
    virtual bool getParamPhysical(const QString &param, QVariant &val) override;
    virtual bool setParamPhysical(const QString &param, const QVariant &val) override;

    virtual void setMotionMaskPhysical(int channel) override;
public:
    static QnPlAreconVisionResource* createResourceByName(const QString &name);
    static QnPlAreconVisionResource* createResourceByTypeId(QnUuid rt);

    static bool isPanoramic(QnResourceTypePtr resType);

protected:
    QString m_description;

private:
    int m_totalMdZones;
};

typedef QnSharedResourcePointer<QnPlAreconVisionResource> QnPlAreconVisionResourcePtr;

#endif

#endif // QnPlAreconVisionResource_h_1252
