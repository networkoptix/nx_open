#ifndef avigilon_device_server_h_1809
#define avigilon_device_server_h_1809

#include "resourcecontrol/abstract_resource_searcher.h"


class AVigilonDeviceServer : public QnAbstractNetworkResourceSearcher
{
	AVigilonDeviceServer();
public:

	~AVigilonDeviceServer();

	static AVigilonDeviceServer& instance();


	// return the name of the server 
	virtual QString name() const;

	// returns all available devices 
    virtual QnResourceList findDevices();

    virtual QnResource* checkHostAddr(QHostAddress addr);

private:

    struct AvigilonCam
    {
        QString ip;
        QString mac;
    };

    QList<AvigilonCam> mCams;

};

#endif // avigilon_device_server_h_1809
