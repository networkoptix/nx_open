#ifndef av_device_server_h_2107
#define av_device_server_h_2107

#include "../../../device/deviceserver.h"

class AVDeviceServer : public CLDeviceServer
{
	AVDeviceServer();
public:

	~AVDeviceServer(){};

	static AVDeviceServer& instance();

	virtual bool isProxy() const;
	// return the name of the server 
	virtual QString name() const;

	// returns all available devices 
	virtual CLDeviceList findDevices();

};

#endif // av_device_server_h_2107
