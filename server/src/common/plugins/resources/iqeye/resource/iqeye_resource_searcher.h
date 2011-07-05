#ifndef iqeye_device_server_h_2054
#define iqeye_device_server_h_2054


#include "resourcecontrol/abstract_resource_searcher.h"


class IQEyeDeviceServer : public QnAbstractNetworkResourceSearcher
{
	IQEyeDeviceServer();
public:

	~IQEyeDeviceServer();

	static IQEyeDeviceServer& instance();


	// return the name of the server 
	virtual QString name() const;

	// returns all available devices 
    virtual QnResourceList findDevices();

    virtual QnResource* checkHostAddr(QHostAddress addr);

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
