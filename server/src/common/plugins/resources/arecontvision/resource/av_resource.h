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

    virtual bool getParam(const QString& name, QnValue& val, QnDomain domain = QnDomainMemory);
    // return true if no error
    virtual bool setParam(const QString& name, const QnValue& val, QnDomain domain = QnDomainMemory);

	virtual bool setHostAddress(const QHostAddress& ip, bool net = true);

	bool getBasicInfo();

	QString toString() const;

	virtual bool getDescription() {return true;};

	CLHttpStatus getRegister(int page, int num, int& val);
	CLHttpStatus setRegister(int page, int num, int val);
	CLHttpStatus setRegister_asynch(int page, int num, int val);

	QnAbstractMediaStreamDataProvider* getDeviceStreamConnection();

	//========
	virtual bool unknownResource() const;
	virtual QnNetworkResourcePtr updateResource();
	//========

protected:

	QnPlAreconVisionResource(int model):
	m_model(model)
	{
	}

	// some AV devices are really challenging to integrate with
	// so this function will do really dirty work about some special cam params
	virtual bool setParam_special(const QString& name, const QnValue& val);

public:
	static QnResourceList findDevices();
	static bool loadDevicesParam(const QString& file_name, QString& error );
private:
	static bool parseDevice(const QDomElement &element, QString& error );
	static bool parseParam(const QDomElement &element, QString& error, QnParamList& paramlist);
	static QnPlAreconVisionResource* deviceByID(QString id, int model);


};

typedef QSharedPointer<QnPlAreconVisionResource> QnPlAreconVisionResourcePtr;

#endif // QnPlAreconVisionResource_h_1252
