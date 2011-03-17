#ifndef fake_device_server_h_2200
#define fake_device_server_h_2200

#include "../../../device/deviceserver.h"

class FakeDeviceServer : public CLDeviceServer
{
	FakeDeviceServer();
public:

	~FakeDeviceServer(){};

	static FakeDeviceServer& instance();

	virtual bool isProxy() const;

	// return the name of the server 
	virtual QString name() const;

	// returns all available devices 
	virtual CLDeviceList findDevices();

};

#endif // av_device_server_h_2107