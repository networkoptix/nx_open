#ifndef device_server_h_1658
#define device_server_h_1658

#include "resource/resource.h"

// this is an interface for camera server server 
// server should be able to provide device list 
// there are two kinds of server:
// proxy 
// not proxy => manages cameras directly 



class CLDeviceServer
{
public:
	CLDeviceServer(){};
	virtual ~CLDeviceServer(){};

	// true means we deal with NVR proxy server; false - with actual devices 
	virtual bool isProxy() const = 0;
	// return the name of the server 
	virtual QString name() const = 0;

	// returns all available devices 
	virtual QnResourceList findDevices() = 0;

};

#endif //device_server_h_1658
