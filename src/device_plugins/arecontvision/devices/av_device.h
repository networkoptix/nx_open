#ifndef av_device_h_1252
#define av_device_h_1252

#include "../../../device/network_device.h"
#include "../../../network/simple_http_client.h"

class QDomElement;


enum 
{	
	AVUNKNOWN = 2000,
	AV1300 = 1300, AV1305 = 1305,
	AV2100 = 2100, AV2105 = 2105, 
	AV3100 = 3100, AV3105 = 3105, 
	AV3130 = 3130, AV3135 = 3135, 
	AV5100 = 5100, AV5105 = 5105, 
	AV8180 = 8180, AV8185 = 8185, 
	AV8360 = 8360, AV8365 = 8365,
	AV10005 = 10005, AV2805 = 2805
};



// this class and inhereted must be very light to create 
class CLAreconVisionDevice : public CLNetworkDevice
{
public:

	// return true if no error
	virtual bool getParam(const QString& name, CLValue& val, bool resynch = false);

	// return true if no error
	virtual bool setParam(const QString& name, const CLValue& val);

	virtual bool setIP(const QHostAddress& ip, bool net = true);

	bool getBaseInfo();

	QString toString() const;

	
	virtual bool getDescription() {return true;};
	
	CLHttpStatus getRegister(int page, int num, int& val);
	CLHttpStatus setRegister(int page, int num, int val);
	CLHttpStatus setRegister_asynch(int page, int num, int val);

	CLStreamreader* getDeviceStreamConnection();

	//========
	virtual bool unknownDevice() const;
	virtual CLNetworkDevice* updateDevice();
	//========

	
	int getModel() const;

	virtual bool executeCommand(CLDeviceCommand* command);

protected:

	CLAreconVisionDevice(int model):
	m_model(model)
	{
	}
	
	// some AV devices are really challenging to integrate with
	// so this function will do really dirty work about some special cam params
	virtual bool setParam_special(const QString& name, const CLValue& val);

public:
	static CLDeviceList findDevices();
	static bool loadDevicesParam(const QString& file_name, QString& error );

protected:
	int m_model;

private:
	static bool parseDevice(const QDomElement &element, QString& error );
	static bool parseParam(const QDomElement &element, QString& error, CLParamList& paramlist);
	static CLAreconVisionDevice* deviceByID(QString id, int model);

	static bool isPanoramic(int model);

	CLAreconVisionDevice(){};



};

#endif // av_device_h_1252