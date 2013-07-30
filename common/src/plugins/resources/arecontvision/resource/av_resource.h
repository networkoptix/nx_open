#ifndef QnPlAreconVisionResource_h_1252
#define QnPlAreconVisionResource_h_1252

#include <QImage>

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/media_data_packet.h"

class QDomElement;

// this class and inherited must be very light to create
class QnPlAreconVisionResource : public QnPhysicalCameraResource
{
    Q_OBJECT

public:
    static const char* MANUFACTURE;

    QnPlAreconVisionResource();

    CLHttpStatus getRegister(int page, int num, int& val);
    CLHttpStatus setRegister(int page, int num, int val);
    CLHttpStatus setRegister_asynch(int page, int num, int val);

    bool isPanoramic() const;
    bool isDualSensor() const;

    virtual bool setHostAddress(const QString& ip, QnDomain domain);

    virtual bool getDescription() {return true;};

    //========
    bool unknownResource() const;
    virtual QnResourcePtr updateResource();
    //========

    virtual QString getDriverName() const override;
    virtual bool isResourceAccessible();
    virtual bool updateMACAddress();

    virtual QnStreamQuality getBestQualityForSuchOnScreenSize(QSize size) const;


    virtual QImage getImage(int channnel, QDateTime time, QnStreamQuality quality);


    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames
    virtual void setCropingPhysical(QRect croping);

    //virtual QnMediaInfo getMediaInfo() const;

    int totalMdZones() const;

protected:
    virtual CameraDiagnostics::Result initInternal() override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    // should just do physical job ( network or so ) do not care about memory domain
    virtual bool getParamPhysical(const QnParam &param, QVariant &val);
    virtual bool setParamPhysical(const QnParam &param, const QVariant &val);

    virtual void setMotionMaskPhysical(int channel) override;
public:
    static QnPlAreconVisionResource* createResourceByName(const QString &name);
    static QnPlAreconVisionResource* createResourceByTypeId(QnId rt);

    static bool isPanoramic(const QString &name);

protected:
    QString m_description;

private:
    int m_totalMdZones;
};

typedef QnSharedResourcePointer<QnPlAreconVisionResource> QnPlAreconVisionResourcePtr;

#endif // QnPlAreconVisionResource_h_1252
