#ifndef iqeye_device_server_h_2054
#define iqeye_device_server_h_2054

#include "../../../device/deviceserver.h"

class IQEyeDeviceServer : public CLDeviceServer
{
	IQEyeDeviceServer();
public:

	~IQEyeDeviceServer();

	static IQEyeDeviceServer& instance();

	virtual bool isProxy() const;
	// return the name of the server 
	virtual QString name() const;

	// returns all available devices 
    virtual CLDeviceList findDevices();

private:

    struct Cam
    {
        QString ip;
        QString mac;
        QString name;
    };

    QList<Cam> mCams;

};

#endif // avigilon_device_server_h_1809
