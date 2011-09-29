#ifndef dlink_device_server_h_1809
#define dlink_device_server_h_1809

#include "device/deviceserver.h"



class DlinkDeviceServer : public CLDeviceServer
{
	DlinkDeviceServer();
public:

	~DlinkDeviceServer();

	static DlinkDeviceServer& instance();

	virtual bool isProxy() const;
	// return the name of the server 
	virtual QString name() const;

	// returns all available devices 
    virtual CLDeviceList findDevices();

};

#endif // avigilon_device_server_h_1809
