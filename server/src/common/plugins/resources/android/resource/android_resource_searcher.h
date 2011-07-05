#ifndef android_device_server_h_2054
#define android_device_server_h_2054


#include "resourcecontrol/abstract_resource_searcher.h"



class AndroidDeviceServer : public QnAbstractNetworkResourceSearcher
{
	AndroidDeviceServer();
public:

	~AndroidDeviceServer();

	static AndroidDeviceServer& instance();


	// return the name of the server 
	virtual QString name() const;

	// returns all available devices 
    virtual QnResourceList findDevices();

    QnResource* checkHostAddr(QHostAddress addr);

private:


};

#endif // avigilon_device_server_h_1809
