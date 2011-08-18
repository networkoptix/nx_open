#ifndef QnPlAreconVisionResource_h_1252
#define QnPlAreconVisionResource_h_1252

#include "resource/network_resource.h"
#include "network/simple_http_client.h"
#include "resource/security_cam_resource.h"


class QDomElement;


// this class and inherited must be very light to create 
class QnPlAreconVisionResource : public QnNetworkResource, public QnSequrityCamResource
{
public:
    QnPlAreconVisionResource();

    CLHttpStatus getRegister(int page, int num, int& val);
    CLHttpStatus setRegister(int page, int num, int val);
    CLHttpStatus setRegister_asynch(int page, int num, int val);

    bool isPanoramic() const;
    bool isDualSensor() const;

	virtual bool setHostAddress(const QHostAddress& ip, QnDomain domain);

    QString toSearchString() const;

	virtual bool getDescription() {return true;};

	//========
	virtual QnResourcePtr updateResource();
	//========

    virtual void beforeUse();
    virtual QString manufacture() const;
    virtual bool isResourceAccessible();
    virtual bool updateMACAddress();

    virtual QnStreamQuality getBestQualityForSuchOnScreenSize(QSize size) const;
    virtual QnCompressedVideoDataPtr getImage(int channnel, QDateTime time, QnStreamQuality quality);
    virtual int getStreamDataProvidersMaxAmount() const;
    virtual QnAbstractMediaStreamDataProvider* createMediaProvider();

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames 
    virtual void setCropingPhysical(QRect croping);

    //virtual QnMediaInfo getMediaInfo() const;

protected:
    // should change value in memory domain 
    virtual bool getParamPhysical(const QString& name, QnValue& val);

    // should just do physical job( network or so ) do not care about memory domain
    virtual bool setParamPhysical(const QString& name, const QnValue& val);
public:
	static QnPlAreconVisionResource* createResourceByName(QString name);
    static bool isPanoramic(QString name);

protected:

    QString m_description;

};

typedef QSharedPointer<QnPlAreconVisionResource> QnPlAreconVisionResourcePtr;

#endif // QnPlAreconVisionResource_h_1252
