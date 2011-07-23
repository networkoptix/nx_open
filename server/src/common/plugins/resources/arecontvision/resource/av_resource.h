#ifndef QnPlAreconVisionResource_h_1252
#define QnPlAreconVisionResource_h_1252

#include "resource/network_resource.h"
#include "network/simple_http_client.h"
#include "resource/media_resource.h"


class QDomElement;


// this class and inherited must be very light to create 
class QnPlAreconVisionResource : public QnNetworkResource, public QnMediaResource
{
public:
    QnPlAreconVisionResource();

    CLHttpStatus getRegister(int page, int num, int& val);
    CLHttpStatus setRegister(int page, int num, int val);
    CLHttpStatus setRegister_asynch(int page, int num, int val);


	virtual bool setHostAddress(const QHostAddress& ip, QnDomain domain);

	bool getBasicInfo();

    QString toSearchString() const;

	virtual bool getDescription() {return true;};

	//========
	virtual QnResourcePtr updateResource();
	//========

    virtual void beforeUse();

protected:
    // should change value in memory domain 
    virtual bool getParamPhysical(const QString& name, QnValue& val);

    // should just do physical job( network or so ) do not care about memory domain
    virtual bool setParamPhysical(const QString& name, const QnValue& val);
public:
	static QnResourceList findDevices();
	static bool loadDevicesParam(const QString& file_name, QString& error );
private:
	static bool parseDevice(const QDomElement &element, QString& error );
	static bool parseParam(const QDomElement &element, QString& error, QnParamList& paramlist);
	static QnPlAreconVisionResource* deviceByID(QString id, int model);

protected:

    QString m_description;

};

typedef QSharedPointer<QnPlAreconVisionResource> QnPlAreconVisionResourcePtr;

#endif // QnPlAreconVisionResource_h_1252
