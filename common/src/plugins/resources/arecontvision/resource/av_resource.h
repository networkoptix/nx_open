#ifndef QnPlAreconVisionResource_h_1252
#define QnPlAreconVisionResource_h_1252

#include "core/resourcemanagment/security_cam_resource.h"
#include "core/resource/network_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/mediadatapacket.h"




class QDomElement;


// this class and inherited must be very light to create 
class QnPlAreconVisionResource : public QnNetworkResource, public QnSequrityCamResource
{
public:
    static const char* MANUFACTURE;

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

    
    virtual QImage getImage(int channnel, QDateTime time, QnStreamQuality quality);


    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames 
    virtual void setCropingPhysical(QRect croping);

    //virtual QnMediaInfo getMediaInfo() const;

    int totalMdZones() const;

protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    // should change value in memory domain 
    virtual bool getParamPhysical(const QString& name, QnValue& val);

    // should just do physical job( network or so ) do not care about memory domain
    virtual bool setParamPhysical(const QString& name, const QnValue& val);
public:
    static QnPlAreconVisionResource* createResourceByName(QString name);
    static QnPlAreconVisionResource* createResourceByTypeId(const QnId& rt);

    static bool isPanoramic(QString name);

protected:

    QString m_description;

private:
    
    int m_totalMdZones;

};

typedef QSharedPointer<QnPlAreconVisionResource> QnPlAreconVisionResourcePtr;

#endif // QnPlAreconVisionResource_h_1252
