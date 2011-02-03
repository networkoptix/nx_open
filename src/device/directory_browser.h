#ifndef directory_browser_h_1708
#define directory_browser_h_1708

#include "deviceserver.h"

class CLDirectoryBrowserDeviceServer : public CLDeviceServer
{
public:
	CLDirectoryBrowserDeviceServer(const QString dir);
	virtual ~CLDirectoryBrowserDeviceServer();

	// true means we deal with NVR proxy server; false - with actual devices 
	virtual bool isProxy() const ;
	// return the name of the server 
	virtual QString name() const ;

	// returns all available devices 
	virtual CLDeviceList findDevices();
protected:
	QString m_directory;
};


#endif //directory_browser_h_1708