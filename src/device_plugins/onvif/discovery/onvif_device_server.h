#ifndef onvif_device_server_h_2054
#define onvif_device_server_h_2054

#include "device/deviceserver.h"


class CLNetworkDevice;

class OnvifDeviceServer : public CLDeviceServer
{
	OnvifDeviceServer();
public:

	~OnvifDeviceServer();

	static OnvifDeviceServer& instance();

	virtual bool isProxy() const;
	// return the name of the server 
	virtual QString name() const;

	// returns all available devices 
    virtual CLDeviceList findDevices();

private:
    CLNetworkDevice* processPacket(CLDeviceList& result, QByteArray& responseData);
    void checkSocket(QUdpSocket& sock, CLDeviceList& result, QHostAddress localAddress);
};

#endif // avigilon_device_server_h_1809
