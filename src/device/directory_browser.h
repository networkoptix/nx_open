#ifndef directory_browser_h_1708
#define directory_browser_h_1708

#include "deviceserver.h"

class CLDeviceDirectoryBrowser : public QThread
{
public:
	CLDeviceDirectoryBrowser();
	virtual ~CLDeviceDirectoryBrowser();

    void setDirList(QStringList& dirs);
    CLDeviceList result();

protected:
    void run();
    CLDeviceList findDevices(const QString& directory);

protected:
    QStringList mDirsToCheck;
    CLDeviceList mResult;

	volatile bool mNeedStop;

};

#endif //directory_browser_h_1708
