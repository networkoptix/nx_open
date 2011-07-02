#ifndef avigilon_device_server_h_1809
#define avigilon_device_server_h_1809

#include "resourcecontrol/resourceserver.h"


class AVigilonDeviceServer : public CLDeviceServer
{
	AVigilonDeviceServer();
public:

	~AVigilonDeviceServer();

	static AVigilonDeviceServer& instance();

	virtual bool isProxy() const;
	// return the name of the server 
	virtual QString name() const;

	// returns all available devices 
    virtual CLDeviceList findDevices();

private:

    struct AvigilonCam
    {
        QString ip;
        QString mac;
    };

    QList<AvigilonCam> mCams;

};

#endif // avigilon_device_server_h_1809
