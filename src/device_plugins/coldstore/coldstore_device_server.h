#ifndef coldstore_device_server_h_2054
#define coldstore_device_server_h_2054

#include "device/deviceserver.h"



class ColdStoreDeviceServer : public CLDeviceServer
{
	ColdStoreDeviceServer ();
public:

	~ColdStoreDeviceServer ();

	static ColdStoreDeviceServer & instance();

	virtual bool isProxy() const;
	// return the name of the server 
	virtual QString name() const;

	// returns all available devices 
    virtual CLDeviceList findDevices();

private:

    void requestFileList(CLDeviceList& result, const QString& csid, const QString& csName, QHostAddress addr);

    QByteArray *m_request;
};

#endif // coldstore_device_server_h_2054
