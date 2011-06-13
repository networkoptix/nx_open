#ifndef android_device_server_h_2054
#define android_device_server_h_2054

#include "../../../device/deviceserver.h"

class AndroidDeviceServer : public CLDeviceServer
{
	AndroidDeviceServer();
public:

	~AndroidDeviceServer();

	static AndroidDeviceServer& instance();

	virtual bool isProxy() const;
	// return the name of the server 
	virtual QString name() const;

	// returns all available devices 
    virtual CLDeviceList findDevices();

private:


};

#endif // avigilon_device_server_h_1809
